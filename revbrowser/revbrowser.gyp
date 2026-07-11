{
	'includes':
	[
		'../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'external-revbrowser',
			'type': 'loadable_module',
			'mac_bundle': 1,
			'product_prefix': '',
			'product_name': 'revbrowser',
			
			'dependencies':
			[
				'../libcore/libcore.gyp:libCore',
				'../libexternal/libexternal.gyp:libExternal',
			],
			
			'include_dirs':
			[
				'src',
			],
			
			'sources':
			[
				'src/osxbrowser.h',
				'src/revbrowser.h',
				'src/revbrowser.rc.h',
				'src/w32browser.h',

				'src/cefbrowser_webview2_stubs.cpp',
				'src/cefbrowser_lnx_stubs.cpp',
				'src/lnxbrowser.cpp',
				'src/osxbrowser.mm',
				'src/revbrowser.cpp',
				'src/w32browser.cpp',
				'src/revbrowser.rc',
			],

			'target_conditions':
			[
				# Only supported on OSX, Windows and Linux
				[
					'not toolset_os in ("mac", "win", "linux")',
					{
						'type': 'none',
					},
				],
				# WebView2 stubs are Windows-only
				[
					'toolset_os != "win"',
					{
						'sources!':
						[
							'src/cefbrowser_webview2_stubs.cpp',
						],
					},
				],
				# Linux stubs are Linux-only
				[
					'toolset_os != "linux"',
					{
						'sources!':
						[
							'src/cefbrowser_lnx_stubs.cpp',
						],
					},
				],
				[
					'toolset_os == "mac"',
					{
						'xcode_settings':
						{
							'OTHER_LDFLAGS': ['-undefined dynamic_lookup'],
						},
						'libraries':
						[
							'$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
							'$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
							'$(SDKROOT)/System/Library/Frameworks/WebKit.framework',
						],
					},
				],
				[
					'toolset_os == "win"',
					{
						'defines':
						[
							'__EXCEPTIONS',
						],
					},
				],
				[
					'toolset_os == "linux"',
					{
						'libraries':
						[
							'-ldl',
							'-lX11',
						],
					},
				],
			],
			
						
			'all_dependent_settings':
			{
				'conditions':
				[
					[
						'OS == "win"',
						{
							'variables':
							{
								'dist_files': [ '<(PRODUCT_DIR)/<(_product_name).dll' ],
							},
						},
					],
					[
						'OS == "linux" and target_arch in ("x86", "x86_64")',
						{
							'variables':
							{
								'dist_files': [ '<(PRODUCT_DIR)/<(_product_name).so' ],
							},
						},
					],
					[
						'OS == "mac"',
						{
							'variables':
							{
								'dist_files': [ '<(PRODUCT_DIR)/<(_product_name).bundle' ],
							},
						},
					],
				],
			},
            
			'cflags_cc!':
			[
				'-fno-rtti',
				'-fno-exceptions',
			],
			
			'msvs_settings':
			{
				'VCCLCompilerTool':
				{
					'ExceptionHandling': '1',	# /EHsc
				},	
			},
			
			'xcode_settings':
			{
				'INFOPLIST_FILE': 'rsrc/revbrowser-Info.plist',
				'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
			},
		},
	],
		
}

