{
	'includes':
	[
		'../../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'libmysql',
			
			'conditions':
			[
				[
					'use_system_libmysql == 0',
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
							'../libz/libz.gyp:libz',
						],
						
						'include_dirs':
						[
							'include',
							'src',
						],
						
						'sources':
						[
							'include/errmsg.h',
							'include/my_alloc.h',
							'include/my_list.h',
							'include/mysql.h',
							'include/mysql_com.h',
							'include/mysql_time.h',
							'include/mysql_version.h',
							'include/typelib.h',
							
							'src/client_settings.h',
							'src/config-android.h',
							'src/config-ios.h',
							'src/config-lnx.h',
							'src/config-osx.h',
							'src/config-win.h',
							'src/m_ctype.h',
							'src/m_string.h',
							'src/my_alarm.h',
							'src/my_attribute.h',
							'src/my_base.h',
							'src/my_charsets.h',
							'src/my_config.h',
							'src/my_dbug.h',
							'src/my_dir.h',
							'src/my_global.h',
							'src/my_net.h',
							'src/my_pthread.h',
							'src/my_static.h',
							'src/my_sys.h',
							'src/my_time.h',
							'src/my_uctype.h',
							'src/my_xml.h',
							'src/mysqld_error.h',
							'src/mysys_err.h',
							'src/mysys_priv.h',
							'src/probes.h',
							'src/sha1.h',
							'src/sql_common.h',
							'src/t_ctype.h',
							'src/thr_alarm.h',
							'src/vio_priv.h',
							'src/violite.h',
							
							'src/array.c',
							'src/bchange.c',
							'src/bmove_upp.c',
							'src/charset.c',
							'src/charset-def.c',
							'src/client.c',
							'src/ctype.c',
							'src/ctype-big5.c',
							'src/ctype-bin.c',
							'src/ctype-cp932.c',
							'src/ctype-czech.c',
							'src/ctype-euc_kr.c',
							'src/ctype-eucjpms.c',
							'src/ctype-extra.c',
							'src/ctype-gb2312.c',
							'src/ctype-gbk.c',
							'src/ctype-latin1.c',
							'src/ctype-mb.c',
							'src/ctype-simple.c',
							'src/ctype-sjis.c',
							'src/ctype-tis620.c',
							'src/ctype-uca.c',
							'src/ctype-ucs2.c',
							'src/ctype-ujis.c',
							'src/ctype-utf8.c',
							'src/ctype-win1250ch.c',
							'src/dbug.c',
							'src/default.c',
							'src/dtoa.c',
							'src/errmsg.c',
							'src/errors.c',
							'src/int2str.c',
							'src/is_prefix.c',
							'src/libmysql.c',
							'src/list.c',
							'src/longlong2str.c',
							'src/mf_arr_appstr.c',
							'src/mf_dirname.c',
							'src/mf_fn_ext.c',
							'src/mf_format.c',
							'src/mf_loadpath.c',
							'src/mf_pack.c',
							'src/mf_qsort.c',
							'src/mulalloc.c',
							'src/my_access.c',
							'src/my_alloc.c',
							'src/my_compress.c',
							'src/my_div.c',
							'src/my_error.c',
							'src/my_fopen.c',
							'src/my_fstream.c',
							'src/my_getsystime.c',
							'src/my_getwd.c',
							'src/my_init.c',
							'src/my_lib.c',
							'src/my_malloc.c',
							'src/my_messnc.c',
							'src/my_once.c',
							'src/my_open.c',
							'src/my_pthread.c',
							'src/my_read.c',
							'src/my_realloc.c',
							'src/my_rnd.c',
							'src/my_static.c',
							'src/my_strtoll10.c',
							'src/my_symlink.c',
							'src/my_sync.c',
							'src/my_thr_init.c',
							'src/my_time.c',
							'src/my_vsnprintf.c',
							'src/my_wincond.c',
							'src/my_winerr.c',
							'src/my_winfile.c',
							'src/net_serv.c',
							'src/pack.c',
							'src/password.c',
							'src/sha1.c',
							'src/str_alloc.c',
							'src/str2int.c',
							'src/strcend.c',
							'src/strend.c',
							'src/strmake.c',
							'src/strmov.c',
							'src/strnlen.c',
							'src/strnmov.c',
							'src/strxmov.c',
							'src/strxnmov.c',
							'src/typelib.c',
							'src/vio.c',
							'src/viosocket.c',
							'src/viossl.c',
							'src/viosslfactories.c',
							'src/xml.c',
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
									'defines':
									[
										# libmysql doesn't expect Win32 to supply struct timespec
										'_CRT_NO_TIME_T',
										'time_t=__time32_t',
									],
									
									'link_settings':
									{
										'libraries':
										[
											'-ladvapi32',
											'-luser32',
										],
									},
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
								'-lmysql',
							],
						},
					},
				],
			],
		},
	],
}
