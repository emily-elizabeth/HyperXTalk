{
	'includes':
	[
		'../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'libbrowser',
			'type': 'static_library',

			'toolsets': ['host', 'target'],
			
			'dependencies':
			[
				# '../libcore/libcore.gyp:libCore',
			],
			
			'include_dirs':
			[
				'include',
				'../libcore/include',
			],

			'sources':
			[
				'include/libbrowser.h',
				
				'src/libbrowser.cpp',
				'src/libbrowser_internal.h',
				'src/libbrowser_memory.cpp',
				'src/libbrowser_value.cpp',

				'src/libbrowser_webview2.cpp',
				'src/libbrowser_webview2.h',
				'src/libbrowser_webview2_win.cpp',

				'src/libbrowser_win.rc.h',
				'src/libbrowser_win.rc',

				'src/libbrowser_osx_webview.h',
				'src/libbrowser_osx_webview.mm',
				
				'src/libbrowser_wkwebview.h',
				'src/libbrowser_wkwebview.mm',

				'src/libbrowser_nsvalue.h',
				'src/libbrowser_nsvalue.mm',
				
				'src/libbrowser_android.cpp',

				'src/libbrowser_webkitgtk.cpp',

				'src/libbrowser_lnx_factories.cpp',
				'src/libbrowser_win_factories.cpp',
				'src/libbrowser_osx_factories.cpp',
				'src/libbrowser_ios_factories.cpp',
			],
			
			'target_conditions':
			[
				[
					'toolset_os != "mac"',
					{
						'sources!':
						[

							'src/libbrowser_osx_webview.h',
							'src/libbrowser_osx_webview.mm',
							
							'src/libbrowser_osx_factories.cpp',
						],
					},
				],
				
				[
					'not toolset_os in ["mac", "ios"]',
					{
						'sources!':
						[
							'src/libbrowser_nsvalue.h',
							'src/libbrowser_nsvalue.mm',
						],
					},
				],
				
				[
					'toolset_os != "win"',
					{
						'sources!':
						[
							'src/libbrowser_win.rc.h',
							'src/libbrowser_win.rc',

							'src/libbrowser_webview2.cpp',
							'src/libbrowser_webview2.h',
							'src/libbrowser_webview2_win.cpp',

							'src/libbrowser_win_factories.cpp',
						],
					},
				],
				
				[
					'toolset_os != "linux"',
					{
						'sources!':
						[
							'src/libbrowser_lnx_factories.cpp',
							'src/libbrowser_webkitgtk.cpp',
						],
					},
				],
				
				[
					'toolset_os != "ios"',
					{
						'sources!':
						[
							'src/libbrowser_ios_factories.cpp',
						],
					},
				],

				# wkwebview sources are shared between iOS and macOS
				[
					'not toolset_os in ["ios", "mac"]',
					{
						'sources!':
						[
							'src/libbrowser_wkwebview.h',
							'src/libbrowser_wkwebview.mm',
						],
					},
				],
				
				[
					'toolset_os != "android"',
					{
						'sources!':
						[
							'src/libbrowser_android.cpp',
						],
					},
				],
			],

			'link_settings':
			{
				'target_conditions':
				[
					[
						'toolset_os == "mac"',
						{
							'libraries':
							[
								'$(SDKROOT)/System/Library/Frameworks/WebKit.framework',
								'$(SDKROOT)/System/Library/Frameworks/JavaScriptCore.framework',
							],
						},
					],
					[
						'toolset_os == "ios"',
						{
							'libraries':
							[
								'$(SDKROOT)/System/Library/Frameworks/WebKit.framework',
							],
						},
					],
				],
			},
			
			# Gyp doesn't like dependencies in 'target_conditions'...
			'conditions':
			[
				[
					'OS == "win"',
					{
						'include_dirs':
						[
							# WebView2 NuGet SDK headers (restored by the CI workflow)
							'../packages/Microsoft.Web.WebView2.1.0.3912.50/build/native/include',
						],
					},
				],
			],
			
			'direct_dependent_settings':
			{
				'include_dirs':
				[
					'include',
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
					'ExceptionHandling': '1',  # /EHsc
				},
			},
			
			
			'xcode_settings':
			{
#				'INFOPLIST_FILE': 'rsrc/libbrowser-Info.plist',
				'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
			},
		},
    ],
}

