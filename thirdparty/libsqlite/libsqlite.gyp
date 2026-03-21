{
	'includes':
	[
		'../../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'libsqlite',
			
			'conditions':
			[
				[
					'use_system_libsqlite == 0',
					{
						'type': 'static_library',
						
						'variables':
						{
							'library_for_module': 1,
							'silence_warnings': 1,
						},
						
						'include_dirs':
						[
							'include',
							'src',
						],
						
						'defines':
						[
							'SQLITE_ENABLE_FTS3',
							'SQLITE_ENABLE_FTS3_PARENTHESIS',
							'SQLITE_ENABLE_FTS4',
							'SQLITE_ENABLE_FTS5',
							'SQLITE_ENABLE_RTREE',
							'SQLITE_ENABLE_JSON1',
						],
						
						'sources':
						[
							'include/sqlite3.h',
							'include/sqlite3ext.h',
							'include/sqlitedecode.h',
							'include/sqlitedataset/dataset.h',
							'include/sqlitedataset/qry_dat.h',
							'include/sqlitedataset/sqlitedataset.h',
							
							'src/dataset.cpp',
							'src/gethostuuid.c',
							'src/qry_dat.cpp',
							'src/sqlite3.c',
							'src/sqlitedataset.cpp',
							'src/sqlitedecode.cpp',
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
								'OS != "mac"',
								{
									'sources!':
									[
										'src/gethostuuid.c',
									],
								},
							],
							[
								'OS != "win"',
								{
									'cflags_cc!':
									[
										'-fno-exceptions',
									],
								},
							],
						],
						
						'msvs_settings':
						{
							'VCCLCompilerTool':
							{
								'ExceptionHandling': 1,
							},
						},
						
						'xcode_settings':
						{
							'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
						},
					},
					{
						'type': 'none',
						
						'link_settings':
						{
							'libraries':
							[
								'-lsqlite',
							],
						},
					},
				],
			],
		},
	],
}
