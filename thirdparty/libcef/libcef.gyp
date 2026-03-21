{
	'includes':
	[
		'../../common.gypi',
		'libcef.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'libcef_library_wrapper',
			'type': 'static_library',

			'toolsets': ['host', 'target'],
			
			'variables':
			{
				'library_for_module': 1,
				'silence_warnings': 1,
			},
			
			# OSX, Windows and Linux only
			'target_conditions':
			[
				[
					'not toolset_os in ("mac", "win", "linux")',
					{
						'type': 'none',
					},
				],
				[
					'toolset_os == "linux" and not toolset_arch in ("x86", "x86_64")',
					{
						'type': 'none',
					},
				],
				[
					'toolset_os == "win"',
					{
						'defines':
						[
							'NOMINMAX',
						],
					},
				],
			],
			
			'include_dirs':
			[
				'.',
			],
		
			'defines':
			[
				# A whole pile of configuration options...
				'ANGLE_DX11',
				'CHROMIUM_BUILD',
				'USE_LIBJPEG_TURBO=1',
				'ENABLE_ONE_CLICK_SIGNIN',
				'ENABLE_REMOTING=1',
				'ENABLE_WEBRTC=1',
				'ENABLE_PEPPER_CDMS',
				'ENABLE_CONFIGURATION_POLICY',
				'ENABLE_INPUT_SPEECH',
				'ENABLE_NOTIFICATIONS',
				'ENABLE_HIDPI=1',
				'ENABLE_GPU=1',
				'ENABLE_EGLIMAGE=1',
				'ENABLE_TASK_MANAGER=1',
				'ENABLE_EXTENSIONS=1',
				'ENABLE_PLUGIN_INSTALLATION=1',
				'ENABLE_PLUGINS=1',
				'ENABLE_SESSION_SERVICE=1',
				'ENABLE_THEMES=1',
				'ENABLE_AUTOFILL_DIALOG=1',
				'ENABLE_BACKGROUND=1',
				'ENABLE_AUTOMATION=1',
				'ENABLE_GOOGLE_NOW=1',
				'CLD_VERSION=1',
				'ENABLE_FULL_PRINTING=1',
				'ENABLE_PRINTING=1',
				'ENABLE_SPELLCHECK=1',
				'ENABLE_CAPTIVE_PORTAL_DETECTION=1',
				'ENABLE_APP_LIST=1',
				'ENABLE_SETTINGS_APP=1',
				'ENABLE_MANAGED_USERS=1',
				'WRAPPING_CEF_SHARED',
				'__STDC_CONSTANT_MACROS',
				'__STDC_FORMAT_MACROS',
				'DYNAMIC_ANNOTATIONS_ENABLED=1',
				'WTF_USE_DYNAMIC_ANNOTATIONS=1',
                'NDEBUG',
			],
			
			'sources':
			[
				'<@(libcef_dll_sources)',
			],

			'direct_dependent_settings':
			{
				'include_dirs':
				[
					'.',
				],
			},
			
			'msvs_settings':
			{
				'VCCLCompilerTool':
				{
					'ExceptionHandling': '1',
				},
			},
		},
		
		{
			'target_name': 'libcef_stubs',
			'type': 'static_library',
			
			'toolsets': ['host', 'target'],

			'variables':
			{
				'library_for_module': 1,
				'silence_warnings': 1,
			},
			
			# OSX, Windows and Linux only
			'target_conditions':
			[
				[
					'not toolset_os in ("mac", "win", "linux")',
					{
						'type': 'none',
					},
				],
			],
			
			'sources':
			[
				'<(INTERMEDIATE_DIR)/src/libcefstubs.cpp',
			],
			
			'actions':
			[
				{
					'action_name': 'generate_libcef_stubs',
					'inputs':
					[
						'../../util/weak_stub_maker.pl',
						'libcef.stubs',
					],
					'outputs':
					[
						'<(INTERMEDIATE_DIR)/src/libcefstubs.cpp',
					],
					
					'action':
					[
						'<@(perl)',
						'../../util/weak_stub_maker.pl',
						'libcef.stubs',
						'<@(_outputs)',
					],
				},
			],
		},
	],
}
