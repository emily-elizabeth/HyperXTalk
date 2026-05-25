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
				'src/cefbrowser.h',
				'src/cefbrowser_msg.h',
				'src/osxbrowser.h',
				'src/revbrowser.h',
				'src/revbrowser.rc.h',
				'src/signal_restore_posix.h',
				'src/w32browser.h',
				
				'src/cefbrowser.cpp',
				'src/cefbrowser_lnx.cpp',
				'src/cefbrowser_w32.cpp',
				'src/cefbrowser_webview2_stubs.cpp',
				'src/cefbrowser_lnx_stubs.cpp',
				'src/lnxbrowser.cpp',
				'src/osxbrowser.mm',
				'src/revbrowser.cpp',
				'src/signal_restore_posix.cpp',
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
				# Real CEF sources not used on any platform; exclude everywhere
				[
					'toolset_os != "never_exists"',
					{
						'sources!':
						[
							'src/cefbrowser.h',
							'src/cefbrowser_msg.h',
							'src/cefbrowser.cpp',
							'src/cefbrowser_lnx.cpp',
							'src/cefbrowser_w32.cpp',
							'src/signal_restore_posix.h',
							'src/signal_restore_posix.cpp',
						],
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

