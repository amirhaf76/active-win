{
	"targets": [
		{
			"target_name": "node-active-win",
			"cflags!": [
				"-fno-exceptions"
			],
			"cflags_cc!": [
				"-fno-exceptions"
			],
			"conditions":[
				[
					"OS=='win'",
					{
						"sources": [
							"sources/windows/main.cc",
						],
						'libraries': [
							'version.lib',
							'Dwmapi.lib',
						],
					},
				],
			],
			"include_dirs": [
				"<!@(node -p \"require('node-addon-api').include\")",
			],
			"defines": [
				"NAPI_VERSION=1", "NAPI_DISABLE_CPP_EXCEPTIONS=1",
			],
			'msvs_settings': {
				'VCCLCompilerTool': {
					'ExceptionHandling': 1,
				},
			},
		}

	]
}
