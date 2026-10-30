{
	'includes':
	[
		'../common.gypi',
	],
	
	'variables':
	{
		'revzip_sources':
		[
			'src/revzip.cpp',
		],
	},
	
	'targets':
	[
		{
			'target_name': 'external-revzip',
			'type': 'loadable_module',
			'mac_bundle': 1,
			'product_prefix': '',
			'product_name': 'revzip',

			'dependencies':
			[
				'../libexternal/libexternal.gyp:libExternal',
                '../libexternal/libexternal.gyp:libExternal-symbol-exports',
				'../thirdparty/libzip/libzip.gyp:libzip',
				'../thirdparty/libz/libz.gyp:libz',
			],

#			'defines':
#			[
				# libzip is linked as a static library; without ZIP_STATIC,
				# zip.h decorates every function with __declspec(dllimport)
				# on Windows, generating __imp_zip_* references that the
				# static libzip.lib cannot satisfy.
#				'ZIP_STATIC',
#			],

#			'include_dirs':
#			[
#				'../thirdparty/libzip/src',
#			],

			'sources':
			[
				'<@(revzip_sources)',
			],
			
			'xcode_settings':
			{
				'INFOPLIST_FILE': 'rsrc/revzip-info.plist',
			},
			
			'conditions':
			[
				[
					'OS == "mac"',
					{
						'libraries':
						[
							'$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
						],
					},
				],
				[
					'OS == "android"',
					{
						'product_name': 'RevZip',
						'product_extension': '',
					},
				],
			],
			
			'all_dependent_settings':
			{
				'variables':
				{
					'dist_files': [ '<(PRODUCT_DIR)/<(_product_name)>(ext_bundle_suffix)' ],
				},
			},
		},
		{
			'target_name': 'external-revzip-server',
			'type': 'loadable_module',
			'product_prefix': '',
			'product_name': 'server-revzip',

			'dependencies':
			[
                '../libexternal/libexternal.gyp:libExternal',
                '../libexternal/libexternal.gyp:libExternal-symbol-exports',
				'../thirdparty/libzip/libzip.gyp:libzip',
				'../thirdparty/libz/libz.gyp:libz',
			],

#			'defines':
#			[
#				'ZIP_STATIC',
#			],

			'sources':
			[
				'<@(revzip_sources)',
			],
			
			'xcode_settings':
			{
				'INFOPLIST_FILE': 'rsrc/revzip-info.plist',
			},
			
			'conditions':
			[
				[
					'OS == "mac"',
					{						
						'libraries':
						[
							'$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
						],
					},
				],
				[
					'OS == "linux"',
					{
						'libraries':
						[
							'-Wl,-Bstatic',
							'-lstdc++',
							'-Wl,-Bdynamic',
						],
					},
				],
			],
			
			'all_dependent_settings':
			{
				'variables':
				{
					'dist_files': [ '<(PRODUCT_DIR)/<(_product_name)>(lib_suffix)' ],
				},
			},
		},
	],

	# OXT-Beyond: revZip for the web engine, which links it into itself
	# (engine/src/em-externals.cpp). Its names, and those of its copy of
	# libExternal, are renamed for it (libexternal/src/external-static.h).
	'conditions':
	[
		[
			'OS == "emscripten"',
			{
				'targets':
				[
					{
						'target_name': 'external-revzip-static',
						'type': 'static_library',

						'dependencies':
						[
							'../libexternal/libexternal.gyp:libExternal-static-revzip',
							'../prebuilt/thirdparty.gyp:thirdparty_prebuilt_zip',
							'../prebuilt/thirdparty.gyp:thirdparty_prebuilt_z',
						],

						'include_dirs':
						[
							'../libexternal/include',
							'../libexternal/src',
						],

						'defines':
						[
							'EXTERNAL_STATIC_PREFIX=revzip',
							# its POSIX code
							'_LINUX',
						],

						'cflags':
						[
							'-include', 'external-static.h',
						],

						'sources':
						[
							'<@(revzip_sources)',
						],
					},
				],
			},
		],
	],
}
