{
	'includes':
	[
		'../../common.gypi',
	],
	
	'target_defaults':
	{
		'conditions':
		[
			[
				'OS != "ios"',
				{
					'variables':
					{
						'ssl_stubs_file': 'ssl.stubs',
					},
				}
			],
			[
				'OS == "ios"',
				{
					'variables':
					{
						'ssl_stubs_file': 'ssl_ios.stubs',
					},
				}
			],
		],
	},

	'targets':
	[
		{
			'target_name': 'libopenssl_stubs',
			'type': 'static_library',

			'dependencies':
			[
				'../../prebuilt/libopenssl.gyp:libopenssl_headers',
			],

			'variables':
			{
				'library_for_module': 1,
			},
			
			'sources':
			[
				'<(INTERMEDIATE_DIR)/src/ssl.<(OS).stubs.cpp',
			],
			
			'actions':
			[
				{
					'action_name': 'generate_libopenssl_stubs',
					'inputs':
					[
						'../../util/weak_stub_maker.pl',
						'>(ssl_stubs_file)',
					],
					'outputs':
					[
						'<(INTERMEDIATE_DIR)/src/ssl.<(OS).stubs.cpp',
					],

					'action':
					[
						'<@(perl)',
						'../../util/weak_stub_maker.pl',
						'>(ssl_stubs_file)',
						'<@(_outputs)',
					],
				},
			],

			'direct_dependent_settings':
			{
				'variables':
				{
					# Default to using the OpenSSL 1.1.0 API
					'openssl_api_compat%': '0x10100000L',
				},
				
				'conditions':
				[
					[
						'OS == "win"',
						{
							'include_dirs':
							[
								'../../prebuilt/unpacked/openssl/<(uniform_arch)-win32-$(PlatformToolset)_static_$(ConfigurationName)/include',
							],
						},
					],
					[
						'OS != "win"',
						{
							'include_dirs':
							[
								'../../prebuilt/include',
							],
						},
					],
				],

				'defines':
				[
					'OPENSSL_API_COMPAT=>(openssl_api_compat)',
				],
			},
		},
        
        {
            'target_name': 'revsecurity_built',
            'type': 'none',

            'dependencies':
            [
                'revsecurity',
            ],
        },
    ],

	'conditions':
	[
		[
			'OS == "mac"',
			{
				'targets':
				[
					{
						'target_name': 'revsecurity',
						'type': 'loadable_module',
						'product_prefix': '',
						'product_name': 'revsecurity',
						'product_extension': 'dylib',

						'dependencies':
						[
							'../../prebuilt/libopenssl.gyp:libopenssl',
							'libopenssl_symbol_list',
						],

						'sources':
						[
							'../../engine/src/dummy.cpp',
						],
						
						'xcode_settings':
						{
							'EXPORTED_SYMBOLS_FILE': '<(SHARED_INTERMEDIATE_DIR)/src/sslstubs.mac.symlist',
						},
						
						'all_dependent_settings':
						{
							'variables':
							{
								'dist_files': [ '<(PRODUCT_DIR)/<(_product_name)>(lib_suffix)' ],
							},
						},
					},
					
					{
						'target_name': 'libopenssl_symbol_list',
						'type': 'none',
						
						'actions':
						[
							{
								'action_name': 'libopenssl_symbol_list',
								'inputs':
								[
									'../../util/list_stub_symbols.pl',
									'>(ssl_stubs_file)',
								],
								'outputs':
								[
									'<(SHARED_INTERMEDIATE_DIR)/src/sslstubs.mac.symlist',
								],
								
								'action':
								[
									'<@(perl)',
									'../../util/list_stub_symbols.pl',
									'_',
									'>(ssl_stubs_file)',
									'<@(_outputs)',
								],
							},
						],
					},
				],
			},
		],
		[
			'OS == "linux" or OS == "android"',
			{
				'targets':
				[
					{
						# Setting this as a loadable module instead of a shared library
						# causes it to get placed next to the built executables rather
						# than in a library sub-directory.
						'target_name': 'revsecurity',
						'type': 'loadable_module',
						'product_prefix': '',
						'product_name': 'revsecurity',

						'dependencies':
						[
							'../../prebuilt/libopenssl.gyp:libopenssl',
						],
						
						'conditions':
						[
							[
								'OS == "android"',
								{
									'product_name': 'RevSecurity',
									'product_extension': '',

									'ldflags!':
									[
										'-flto',
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
			},
		],
		[
			'OS == "win"',
			{
				'targets':
				[
					# Revsecurity is a pre-built on windows
					{
						'target_name': 'revsecurity',
						'type': 'shared_library',
						'product_name': 'revsecurity',
						
						'dependencies':
						[
							'../../prebuilt/libopenssl.gyp:libopenssl',
							'libopenssl_symbol_list_win',

						],
						
						'sources':
						[
							'src/dummy.cpp',
						],
						
						'libraries':
						[
							'-lws2_32.lib',
							'-lgdi32.lib',
							'-ladvapi32.lib',
							'-lcrypt32.lib',
							'-luser32.lib',
						],

						'msvs_settings':
						{
							'VCLinkerTool':
							{
								'ModuleDefinitionFile': '<(SHARED_INTERMEDIATE_DIR)/src/revsecurity.def',
								'SubSystem': '1',
							},
							
							'VCManifestTool':
							{
								# Seems to be missing...
								#'AdditionalManifestFiles': 'revsecurity.manifest',
							},
						},
						
						'all_dependent_settings':
						{
							'variables':
							{
								'dist_files': [ '<(PRODUCT_DIR)/<(_product_name)>(lib_suffix)' ],
							},
						},
					},
					{
						'target_name': 'libopenssl_symbol_list_win',
						'type': 'none',
						
						'actions':
						[
							{
								'action_name': 'libopenssl_symbol_list_win',
								'inputs':
								[
									'../../util/list_stub_symbols.pl',
									'>(ssl_stubs_file)',
								],
								'outputs':
								[
									'<(SHARED_INTERMEDIATE_DIR)/src/revsecurity.def',
								],
								
								'action':
								[
									'<@(perl)',
									'../../util/list_stub_symbols.pl',
									'',
									'>(ssl_stubs_file)',
									'<@(_outputs)',
									'--exportdef=REVSECURITY',
								],
							},
						],
					},
				],
			}
		],
		[
			'OS == "ios"',
			{
				'targets':
				[
					{
						'target_name': 'revsecurity',
						'type': 'loadable_module',
						'product_prefix': '',
						'product_name': 'revsecurity',

						'dependencies':
						[
							'../../prebuilt/libopenssl.gyp:libopenssl',
							'libopenssl_symbol_list',
						],

						'sources':
						[
							'<(SHARED_INTERMEDIATE_DIR)/src/libopenssl-libinfo.ios.cpp',
						],
						
						'xcode_settings':
						{
							'EXPORTED_SYMBOLS_FILE': '<(SHARED_INTERMEDIATE_DIR)/src/sslstubs.mac.symlist',
						},
						
						'variables':
						{
							'ios_external_symbols': [],
							'ios_external_symbol_list': '<(SHARED_INTERMEDIATE_DIR)/src/sslstubs.ios.symlist',
						},
						
						'all_dependent_settings':
						{
							'variables':
							{
								'dist_files': [ '<(PRODUCT_DIR)/<(_product_name)>(ext_bundle_suffix)' ],
							},
						},
					},
					
					{
						'target_name': 'libopenssl_symbol_list',
						'type': 'none',
						
						'actions':
						[
							{
								'action_name': 'libopenssl_symbol_list',
								'inputs':
								[
									'../../util/list_stub_symbols.pl',
									'>(ssl_stubs_file)',
								],
								'outputs':
								[
									'<(SHARED_INTERMEDIATE_DIR)/src/sslstubs.ios.symlist',
								],
								
								'action':
								[
									'<@(perl)',
									'../../util/list_stub_symbols.pl',
									'_',
									'>(ssl_stubs_file)',
									'<@(_outputs)',
								],
							},
							{
								'action_name': 'libopenssl_libinfo',
								'inputs':
								[
									'../../util/build_libinfo.pl',
									'>(ssl_stubs_file)',
								],
								'outputs':
								[
									'<(SHARED_INTERMEDIATE_DIR)/src/libopenssl-libinfo.ios.cpp',
								],
								
								'action':
								[
									'<@(perl)',
									'../../util/build_libinfo.pl',
									'revsecurity',
									'>(ssl_stubs_file)',
									'<@(_outputs)',
								],
							},
						],
					},
				],
			},
		],
		[
			'OS == "emscripten"',
			{
				'targets':
				[
					{
						# Dummy revsecurity target for emscripten
						'target_name': 'revsecurity',
						'type': 'none',
					},
				],
			},
		],
	],
}
