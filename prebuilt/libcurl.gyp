{
	'includes':
	[
		'../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'libcurl',
			'type': 'none',
			
			'toolsets': ['host', 'target'],

			'dependencies':
			[
				'fetch.gyp:fetch',
				'libopenssl.gyp:libopenssl',
			],
			
			'direct_dependent_settings':
			{
				'target_conditions':
				[
					[
						'toolset_os == "win"',
						{
							'include_dirs':
							[
								'unpacked/curl/<(uniform_arch)-win32-$(PlatformToolset)_static_$(ConfigurationName)/include',
							],
						},
					],
					[
						'toolset_os != "win"',
						{
							'include_dirs':
							[
								'../thirdparty/libcurl/include',
							],
						},
					],
				],
			},
			
			'link_settings':
			{
				'target_conditions':
				[
					[
						'toolset_os == "mac"',
						{
							'libraries':
							[
								'$(SDKROOT)/usr/lib/libcurl.dylib',
							],
						},
					],
					[
						'toolset_os == "linux"',
						{
							'library_dirs':
							[
								'lib/linux/>(toolset_arch)',
							],

							'libraries':
							[
								'-lcurl',
								'-lrt',
							],
						},
					],
				],
			},

			# On Windows the GYP MSVS generator rejects link_settings inside
			# target_conditions ("not allowed in the Debug configuration").
			# The workaround — used by libxml.gyp for bcrypt.lib — is to place
			# link_settings at the TOP LEVEL of all_dependent_settings, gated by
			# a parse-time OS condition (not a per-configuration target_condition).
			# This makes every target that (transitively) depends on libcurl
			# inherit the library dir and the required import libs.
			'conditions':
			[
				[
					'OS == "win"',
					{
						'all_dependent_settings':
						{
							'link_settings':
							{
								'library_dirs':
								[
									'unpacked/curl/<(uniform_arch)-win32-$(PlatformToolset)_static_$(ConfigurationName)/lib',
								],

								'libraries':
								[
									'-llibcurl_a',
									'-lws2_32',
									'-lwldap32',
									'-lcrypt32',
									# WinSSL / schannel backend (used by vcpkg's curl):
									'-lbcrypt',   # BCryptGenRandom (rand.c)
									'-lsecur32',  # InitSecurityInterfaceW (curl_sspi.c)
								],
							},
						},
					},
				],
			],
		},
	],
}
