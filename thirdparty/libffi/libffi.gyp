{
	'includes':
	[
		'../../common.gypi',
	],
	
	'variables':
	{
		'libffi_public_headers_darwin_osx_dir':
		[
			'./include_darwin'
		],
		
		'libffi_public_headers_darwin_ios_dir':
		[
			'./git_master/darwin_ios/include',
			'./git_master/darwin_common/include',
		],
		
		'libffi_public_headers_win32_dir':
		[
			'./include_win32',
		],

		'libffi_public_headers_win64_dir':
		[
			'./include_win64',
		],
		
		'libffi_public_headers_linux_x86_dir':
		[
			'./include_linux/x86',
		],
		
		'libffi_public_headers_linux_x86_64_dir':
		[
			'./include_linux/x86_64',
		],

		'libffi_public_headers_linux_arm64_dir':
		[
			'./include_linux/arm64',
		],
		
		'libffi_public_headers_android_dir':
		[
			'./include_android',
		],

		'libffi_generic_sources':
		[
			'src/closures.c',
			'src/debug.c',
			'src/java_raw_api.c',
			'src/prep_cif.c',
			'src/raw_api.c',
			'src/types.c',
		],
		
		'libffi_mac_source_files':
		[
			'src/x86/darwin.S',
			'src/x86/darwin64.S',
			'src/x86/ffi.c',
			'src/x86/ffi64.c',
			'src/x86/win32.S',
		],
		
		'libffi_ios_source_files':
		[
			'git_master/darwin_ios/src/aarch64/ffi_arm64.c',
			'git_master/darwin_ios/src/aarch64/sysv_arm64.S',
			'git_master/darwin_ios/src/arm/ffi_armv7.c',
			'git_master/darwin_ios/src/arm/sysv_armv7.S',
			'git_master/darwin_ios/src/arm/trampoline_armv7.S',
			'git_master/darwin_ios/src/x86/ffi_i386.c',
			'git_master/darwin_ios/src/x86/sysv_i386.S',

			'git_master/src/x86/ffi64.c',
			'git_master/src/x86/unix64.S',
			'git_master/src/x86/ffiw64.c',
			'git_master/src/x86/win64.S',

			'git_master/src/closures.c',
			'git_master/src/debug.c',
			'git_master/src/java_raw_api.c',
			'git_master/src/prep_cif.c',
			'git_master/src/raw_api.c',
			'git_master/src/types.c',
		],
		
		'libffi_win32_source_files':
		[
			'src/x86/ffi.c',
			'src/x86/win32.asm',
		],

		'libffi_win64_source_files':
		[
			'src/x86/ffi.c',
			'src/x86/ffi64.c',
			'src/x86/win64.asm',
		],
		
		'libffi_linux_x86_source_files':
		[
			'src/x86/ffi.c',
			'src/x86/ffi64.c',
			'src/x86/sysv.S',
			'src/x86/unix64.S',
			'src/x86/win32.S',
		],

		'libffi_emscripten_source_files':
		[
			'src/x86/ffi.c',
		],

		'libffi_linux_arm_source_files':
		[
			'src/arm/ffi.c',
			'src/arm/sysv.S',
			'src/arm/trampoline.S',
		],

		'libffi_linux_arm64_source_files':
		[
			'src/aarch64/ffi.c',
			'src/aarch64/sysv.S',
		],
	},
	
	'targets':
	[
		{
			'target_name': 'libffi',
			'type': 'static_library',
			
			'toolsets': ['host','target'],
			
			'product_prefix': '',
			'product_name': 'libffi',
			
			'variables':
			{
				'conditions':
				[
					[
						'_toolset == "host"',
						{
							'toolset_os': '<(host_os)',
							'toolset_arch': '<(host_arch)',
						},
						{
							'toolset_os': '<(OS)',
							'toolset_arch': '<(target_arch)',
						},
					],
				],

				'silence_warnings': 1,
			},
			
			'sources':
			[
			],
			
			'include_dirs':
			[
				'<@(_platform_include_dirs)',
			],
			
			'direct_dependent_settings':
			{
				'include_dirs':
				[
					'<@(_platform_include_dirs)',
				],

				'defines':
				[
					# Ensures we don't try to DLLImport symbols from a static lib
					'FFI_BUILDING',
				],
			},
			
			'all_dependent_settings':
			{
				'msvs_settings':
				{
					'VCLinkerTool':
					{
						# libffi is not safe exception handler compatible therefore nothing
						# linked to it is compatible either
						'ImageHasSafeExceptionHandlers': 'false',
					},
				},
			},
			
			'conditions':
			[
				[
					'toolset_os == "mac"',
					{
						'platform_include_dirs':
						[
							'<@(libffi_public_headers_darwin_osx_dir)',
						],
						
						'sources':
						[
							'<@(libffi_mac_source_files)',
							'<@(libffi_generic_sources)'
						],
					},
				],
				[
					'toolset_os == "ios"',
					{
						'platform_include_dirs':
						[
							'<@(libffi_public_headers_darwin_ios_dir)',
						],
						
						'sources':
						[
							'<@(libffi_ios_source_files)',
							'<@(libffi_generic_sources)'
						],

						'include_dirs':
						[
							'git_master/src',
						],
					},
				],
				[
					'toolset_os == "win" and toolset_arch == "x86"',
					{
						'platform_include_dirs':
						[
							'<@(libffi_public_headers_win32_dir)',
						],
						
						'sources':
						[
							'<@(libffi_win32_source_files)',
							'<@(libffi_generic_sources)'
						],
					},
				],
				[
					'toolset_os == "win" and toolset_arch == "x64"',
					{
						'platform_include_dirs':
						[
							'<@(libffi_public_headers_win64_dir)',
						],
						
						'sources':
						[
							'<@(libffi_win64_source_files)',
							'<@(libffi_generic_sources)'
						],
					},
				],
				[
					'(toolset_os == "linux" or toolset_os == "android") and toolset_arch == "x86"',
					{
						'platform_include_dirs':
						[
							'<@(libffi_public_headers_linux_x86_dir)',
						],
						
						'sources':
						[
							'<@(libffi_linux_x86_source_files)',
							'<@(libffi_generic_sources)'
						],
					},
				],
				[
					'(toolset_os == "linux" or toolset_os == "android") and toolset_arch == "x86_64"',
					{
						'platform_include_dirs':
						[
							'<@(libffi_public_headers_linux_x86_64_dir)',
						],
						
						'sources':
						[
							'<@(libffi_linux_x86_source_files)',
							'<@(libffi_generic_sources)'
						],
					},
				],
				[
					'toolset_os in ("linux", "android") and toolset_arch in ("armv6", "armv6hf", "armv7")',
					{
						'platform_include_dirs':
						[
							'<@(libffi_public_headers_android_dir)',
						],
						
						'sources':
						[
							'<@(libffi_linux_arm_source_files)',
							'<@(libffi_generic_sources)'
						],
						
						# Disable VFP for non-hard-float targets
                        'conditions':
                        [
                            [
                                'toolset_arch == "armv6"',
                                {
                                    'cflags':
                                    [
                                        '-U__ARM_EABI__',
                                    ],
                                },
                            ],
                        ],
					},
				],
				[
					'toolset_os in ("linux", "android") and toolset_arch == "arm64"',
					{
						'platform_include_dirs':
						[
							'<@(libffi_public_headers_linux_arm64_dir)',
						],

						'sources':
						[
							'<@(libffi_linux_arm64_source_files)',
							'<@(libffi_generic_sources)',
						],
					},
				],
				[
					'toolset_os == "emscripten"',
					{
						'platform_include_dirs':
						[
							'<@(libffi_public_headers_linux_x86_dir)',
						],

						'sources':
						[
							'<@(libffi_emscripten_source_files)',
							'<@(libffi_generic_sources)'
						]
					},
				],
			],
			
			'target_conditions':
			[
				[
					'_toolset != "target"',
					{
						'product_name': 'libffi->(_toolset)',
					},
				],
			],
		},
	],
}
