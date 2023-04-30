#include <cmath>
#include <cstdint>
#include <napi.h>
#include <iostream>
#include <shtypes.h>
#include <string>
#include <windows.h>
#include <psapi.h>
#include <version>
#include <dwmapi.h>
#include <algorithm>
#include <AtlBase.h>
#include <UIAutomation.h>
#include <comutil.h>

typedef int(__stdcall *lp_GetScaleFactorForMonitor)(HMONITOR, DEVICE_SCALE_FACTOR *);

struct OwnerWindowInfo {
	std::string path;
	std::string name;
};

template <typename T>
T getValueFromCallbackData(const Napi::CallbackInfo &info, unsigned handleIndex) {
	return reinterpret_cast<T>(info[handleIndex].As<Napi::Number>().Int64Value());
}

// Get wstring from string
std::wstring get_wstring(const std::string str) {
	return std::wstring(str.begin(), str.end());
}

std::string getFileName(const std::string &value) {
	char separator = '/';
#ifdef _WIN32
	separator = '\\';
#endif
	size_t index = value.rfind(separator, value.length());

	if (index != std::string::npos) {
		return (value.substr(index + 1, value.length() - index));
	}

	return ("");
}

// Convert wstring into utf8 string
std::string toUtf8(const std::wstring &str) {
	std::string ret;
	int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0, NULL, NULL);
	if (len > 0) {
		ret.resize(len);
		WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.length(), &ret[0], len, NULL, NULL);
	}

	return ret;
}

// Return window title in utf8 string
std::string getWindowTitle(const HWND hwnd) {
	int bufsize = GetWindowTextLengthW(hwnd) + 1;
	LPWSTR t = new WCHAR[bufsize];
	GetWindowTextW(hwnd, t, bufsize);

	std::wstring ws(t);
	std::string title = toUtf8(ws);

	return title;
}

// Return description from file version info
std::string getDescriptionFromFileVersionInfo(const BYTE *pBlock) {
	UINT bufLen = 0;
	struct LANGANDCODEPAGE {
		WORD wLanguage;
		WORD wCodePage;
	} * lpTranslate;

	LANGANDCODEPAGE codePage{0x040904E4};
	// Get language struct
	if (VerQueryValueW((LPVOID *)pBlock, (LPCWSTR)L"\\VarFileInfo\\Translation", (LPVOID *)&lpTranslate, &bufLen)) {
		codePage = lpTranslate[0];
	}

	wchar_t fileDescriptionKey[256];
	wsprintfW(fileDescriptionKey, L"\\StringFileInfo\\%04x%04x\\FileDescription", codePage.wLanguage, codePage.wCodePage);
	wchar_t *fileDescription = NULL;
	UINT fileDescriptionSize;
	// Get description file
	if (VerQueryValueW((LPVOID *)pBlock, fileDescriptionKey, (LPVOID *)&fileDescription, &fileDescriptionSize)) {
		return toUtf8(fileDescription);
	}

	return "";
}

std::string chrome_geturl(HWND hwnd) {

	CoInitialize(NULL);
	CComBSTR empty(L"");
	BSTR r = empty;
	while (true)
	{
		if (!hwnd)
			break;
		if (!IsWindowVisible(hwnd))
			continue;


		CComQIPtr<IUIAutomation> uia;
		if (FAILED(uia.CoCreateInstance(CLSID_CUIAutomation)) || !uia)
			break;

		CComPtr<IUIAutomationElement> root;
		if (FAILED(uia->ElementFromHandle(hwnd, &root)) || !root)
			break;


		CComPtr<IUIAutomationCondition> condition;

		//URL's id is 0xC354, or use UIA_EditControlTypeId for 1st edit box
		uia->CreatePropertyCondition(UIA_ControlTypePropertyId,
			CComVariant(0xC354), &condition);

		//or use edit control's name instead
		//uia->CreatePropertyCondition(UIA_NamePropertyId,
		//      CComVariant(L"Address and search bar"), &condition);

		CComPtr<IUIAutomationElement> edit;
		if (FAILED(root->FindFirst(TreeScope_Descendants, condition, &edit))
			|| !edit)
			break; //maybe we don't have the right tab, continue...

		CComVariant url;
		if (FAILED(edit->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &url)))
			break;
		r = url.bstrVal;
		break;
	}
	CoUninitialize();

	std::wstring ws(r);
	std::string url = toUtf8(ws);

	return url;

};

std::string mozila_geturl(HWND hwnd)
{
	CoInitialize(NULL);
	CComBSTR empty(L"");
	BSTR r = empty;

	while (true)
	{
		if (!hwnd)
			break;
		if (!IsWindowVisible(hwnd))
			continue;

		CComQIPtr<IUIAutomation> uia;
		if (FAILED(uia.CoCreateInstance(CLSID_CUIAutomation)) || !uia)
			break;

		CComPtr<IUIAutomationElement> element;
		if (FAILED(uia->ElementFromHandle(hwnd, &element)) || !element)
			break;

		//initialize conditions
		CComPtr<IUIAutomationCondition> toolbar_cond;
		uia->CreatePropertyCondition(UIA_ControlTypePropertyId,
			CComVariant(UIA_ToolBarControlTypeId), &toolbar_cond);

		CComPtr<IUIAutomationCondition> combobox_cond;
		uia->CreatePropertyCondition(UIA_ControlTypePropertyId,
			CComVariant(UIA_ComboBoxControlTypeId), &combobox_cond);

		CComPtr<IUIAutomationCondition> editbox_cond;
		uia->CreatePropertyCondition(UIA_ControlTypePropertyId,
			CComVariant(UIA_EditControlTypeId), &editbox_cond);

		//find the top toolbars
		CComPtr<IUIAutomationElementArray> toolbars;
		if (FAILED(element->FindAll(TreeScope_Children, toolbar_cond, &toolbars)) || !toolbars)
			break;

		int toolbars_count = 0;
		toolbars->get_Length(&toolbars_count);
		for (int i = 0; i < toolbars_count; i++)
		{
			CComPtr<IUIAutomationElement> toolbar;
			if (FAILED(toolbars->GetElement(i, &toolbar)) || !toolbar)
				continue;

			//find the comboxes for each toolbar
			CComPtr<IUIAutomationElementArray> comboboxes;
			if (FAILED(toolbar->FindAll(TreeScope_Children, combobox_cond, &comboboxes)) || !comboboxes)
				break;

			int combobox_count = 0;
			comboboxes->get_Length(&combobox_count);
			for (int j = 0; j < combobox_count; j++)
			{
				CComPtr<IUIAutomationElement> combobox;
				if (FAILED(comboboxes->GetElement(j, &combobox)) || !combobox)
					continue;

				CComVariant test;
				if (FAILED(combobox->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &test)))
					continue;

				//we are interested in a combobox which has no lable
				if (wcslen(test.bstrVal))
					continue;

				//find the first editbox
				CComPtr<IUIAutomationElement> edit;
				if (FAILED(combobox->FindFirst(TreeScope_Descendants, editbox_cond, &edit)) || !edit)
					continue;

				CComVariant bstr;
				if (FAILED(edit->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &bstr)))
					continue;
				r = bstr.bstrVal;
				break;
			}
		}
		break;
	}

	CoUninitialize();
	
	std::wstring ws(r);
	std::string url = toUtf8(ws);

	return url;
}



// Return process path and name
OwnerWindowInfo getProcessPathAndName(const HANDLE &phlde) {
	DWORD dwSize{MAX_PATH};
	wchar_t exeName[MAX_PATH]{};
	QueryFullProcessImageNameW(phlde, 0, exeName, &dwSize);
	std::string path = toUtf8(exeName);
	std::string name = getFileName(path);

	DWORD dwHandle = 0;
	wchar_t *wspath(exeName);
	DWORD infoSize = GetFileVersionInfoSizeW(wspath, &dwHandle);

	if (infoSize != 0) {
		BYTE *pVersionInfo = new BYTE[infoSize];
		std::unique_ptr<BYTE[]> skey_automatic_cleanup(pVersionInfo);
		if (GetFileVersionInfoW(wspath, NULL, infoSize, pVersionInfo) != 0) {
			std::string nname = getDescriptionFromFileVersionInfo(pVersionInfo);
			if (nname != "") {
				name = nname;
			}
		}
	}

	return {path, name};
}

OwnerWindowInfo newOwner;

BOOL CALLBACK EnumChildWindowsProc(HWND hwnd, LPARAM lParam) {
	// Get process ID
	DWORD processId{0};
	GetWindowThreadProcessId(hwnd, &processId);
	OwnerWindowInfo *ownerInfo = (OwnerWindowInfo *)lParam;
	// Get process Handler
	HANDLE phlde{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, processId)};

	if (phlde != NULL) {
		newOwner = getProcessPathAndName(phlde);
		CloseHandle(hwnd);
		if (ownerInfo->path != newOwner.path) {
			return FALSE;
		}
	}

	return TRUE;
}

// Return window information
Napi::Value getWindowInformation(const HWND &hwnd, const Napi::CallbackInfo &info) {
	Napi::Env env{info.Env()};

	if (hwnd == NULL) {
		return env.Null();
	}

	// Get process ID
	DWORD processId{0};
	GetWindowThreadProcessId(hwnd, &processId);
	// Get process Handler
	HANDLE phlde{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, processId)};

	if (phlde == NULL) {
		return env.Null();
	}

	OwnerWindowInfo ownerInfo = getProcessPathAndName(phlde);

	// ApplicationFrameHost & Universal Windows Platform Support
	if (getFileName(ownerInfo.path) == "ApplicationFrameHost.exe") {
		newOwner = (OwnerWindowInfo) * new OwnerWindowInfo();
		BOOL result = EnumChildWindows(hwnd, (WNDENUMPROC)EnumChildWindowsProc, (LPARAM)&ownerInfo);
		if (result == FALSE && newOwner.name.size())
		{
			ownerInfo = newOwner;
		}
	}

	if (ownerInfo.name == "Widgets.exe") {
		return env.Null();
	}

	PROCESS_MEMORY_COUNTERS memoryCounter;
	BOOL memoryResult = GetProcessMemoryInfo(phlde, &memoryCounter, sizeof(memoryCounter));

	CloseHandle(phlde);

	if (memoryResult == 0) {
		return env.Null();
	}

	RECT lpRect;
	BOOL rectResult = GetWindowRect(hwnd, &lpRect);

	if (rectResult == 0) {
		return env.Null();
	}

	Napi::Object owner = Napi::Object::New(env);

	owner.Set(Napi::String::New(env, "processId"), processId);
	owner.Set(Napi::String::New(env, "path"), ownerInfo.path);
	owner.Set(Napi::String::New(env, "name"), ownerInfo.name);

	Napi::Object bounds = Napi::Object::New(env);

	bounds.Set(Napi::String::New(env, "x"), lpRect.left);
	bounds.Set(Napi::String::New(env, "y"), lpRect.top);
	bounds.Set(Napi::String::New(env, "width"), lpRect.right - lpRect.left);
	bounds.Set(Napi::String::New(env, "height"), lpRect.bottom - lpRect.top);

	Napi::Object activeWinObj = Napi::Object::New(env);

	activeWinObj.Set(Napi::String::New(env, "platform"), Napi::String::New(env, "windows"));
	activeWinObj.Set(Napi::String::New(env, "id"), (LONG)hwnd);
	activeWinObj.Set(Napi::String::New(env, "title"), getWindowTitle(hwnd));
	activeWinObj.Set(Napi::String::New(env, "owner"), owner);
	activeWinObj.Set(Napi::String::New(env, "bounds"), bounds);
	activeWinObj.Set(Napi::String::New(env, "memoryUsage"), memoryCounter.WorkingSetSize);
	if(ownerInfo.name == "Firefox"){
		activeWinObj.Set(Napi::String::New(env, "url"), mozila_geturl(hwnd));
	}else{
		activeWinObj.Set(Napi::String::New(env, "url"), chrome_geturl(hwnd));
	}

	return activeWinObj;
}

// List of HWND used for EnumDesktopWindows callback
std::vector<HWND> _windows;

// EnumDesktopWindows callback
BOOL CALLBACK EnumDekstopWindowsProc(HWND hwnd, LPARAM lParam) {
	if (IsWindow(hwnd) && IsWindowEnabled(hwnd) && IsWindowVisible(hwnd)) {
		WINDOWINFO winInfo{};
		GetWindowInfo(hwnd, &winInfo);

		if (
			(winInfo.dwExStyle & WS_EX_TOOLWINDOW) == 0
			&& (winInfo.dwStyle & WS_CAPTION) == WS_CAPTION
			&& (winInfo.dwStyle & WS_CHILD) == 0
		) {
			int ClockedVal;
			DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, (PVOID)&ClockedVal, sizeof(ClockedVal));
			if (ClockedVal == 0) {
				_windows.push_back(hwnd);
			}
		}
	}

	return TRUE;
};

// Method to get active window
Napi::Value getActiveWindow(const Napi::CallbackInfo &info) {
	HWND hwnd = GetForegroundWindow();
	Napi::Value value = getWindowInformation(hwnd, info);
	return value;
}

// Method to get an array of open windows
Napi::Array getOpenWindows(const Napi::CallbackInfo &info) {
	Napi::Env env{info.Env()};

	Napi::Array values = Napi::Array::New(env);

	_windows.clear();

	if (EnumDesktopWindows(NULL, (WNDENUMPROC)EnumDekstopWindowsProc, NULL)) {
		uint32_t i = 0;
		for (HWND _win : _windows) {
			Napi::Value value = getWindowInformation(_win, info);
			if (value != env.Null()) {
				values.Set(i++, value);
			}
		}
	}

	return values;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
	exports.Set(Napi::String::New(env, "getActiveWindow"), Napi::Function::New(env, getActiveWindow));
	exports.Set(Napi::String::New(env, "getOpenWindows"), Napi::Function::New(env, getOpenWindows));
	return exports;
}

NODE_API_MODULE(addon, Init)
