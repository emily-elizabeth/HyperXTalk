{
	'includes':
	[
		'../../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'libpq',
			
			'conditions':
			[
				[
					'use_system_libpq == 0',
					{
						'type': 'static_library',
						
						'variables':
						{
							'library_for_module': 1,
							'silence_warnings': 1,

							# OpenSSL 1.0.1 API
							'openssl_api_compat': '0x10000100L',
						},
                        
						'dependencies':
						[
							'../libopenssl/libopenssl.gyp:libopenssl_stubs',
						],
						
						'include_dirs':
						[
							'include',
							'src',
						],
						
						'defines':
						[
							'HAVE_CONFIG_H',
						],
						
						'sources':
						[
							'include/libpq-fe.h',
							'include/pg_config_ext.h',
							'include/postgres_ext.h',

							'src/pg_config.h',
							'src/pg_config.mac.h',
							'src/pg_config.win32.h',
							'src/pg_config.linux.h',

							'src/chklocale.c',
							'src/encnames.c',
							'src/fe-auth.c',
							'src/fe-connect.c',
							'src/fe-exec.c',
							'src/fe-lobj.c',
							'src/fe-misc.c',
							'src/fe-print.c',
							'src/fe-protocol2.c',
							'src/fe-protocol3.c',
							'src/fe-secure.c',
							'src/getpeereid.c',
							'src/inet_net_ntop.c',
							'src/ip.c',
							'src/libpq-events.c',
							'src/md5.c',
							'src/noblock.c',
							'src/pgsleep.c',
							'src/pgstrcasecmp.c',
							'src/pqexpbuffer.c',
							'src/pqsignal.c',
							'src/strlcpy.c',
							'src/thread.c',
							'src/wchar.c',
						],

						
						'direct_dependent_settings':
						{
							'include_dirs':
							[
								'include',
							],
						},
						
						'conditions':
						[
							[
								'OS == "win"',
								{
									'include_dirs':
									[
										'src/port/win32',
										'src/port/win32_msvc',
									],

									'sources':
									[
										'src/crypt.c',
										'src/dirent.c',
										'src/dirmod.c',
										'src/getaddrinfo.c',
										'src/inet_aton.c',
										'src/pthread-win32.c',
										'src/open.c',
										'src/snprintf.c',
										'src/system.c',
										'src/win32.c',
										'src/win32setlocale.c',
										'src/win32error.c',
									],
									
									'link_settings':
									{
										'libraries':
										[
											'-ladvapi32',
											'-lsecur32',
											'-lshell32',
											'-lwldap32',
											'-lws2_32',
											'-lwsock32',
										],
									},
								}
							],
							[
								'OS == "mac"',
								{
									'sources!':
									[
										'src/strlcpy.c',
									],
								},
							],
						],
					},
					{
						'type': 'none',
						
						'link_settings':
						{
							'libraries':
							[
								'-lpq',
							],
						},
					},
				],
			],
		},
	],
}
