{
	'includes':
	[
		'../../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'libzip',
			
			'dependencies':
			[
				'../libz/libz.gyp:libz',
			],
			
			'conditions':
			[
				[
					'use_system_libzip == 0',
					{
						'target_name': 'libzip',
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
						
						'sources':
						[
							'include/zip.h',
							
							'src/zipint.h',
							'src/zipintw32.h',
							
							'src/w32support.cpp',
							
							'src/mkstemp.c',
							'src/zip_add.c',
							'src/zip_close.c',
							'src/zip_delete.c',
							'src/zip_dirent.c',
							'src/zip_entry_free.c',
							'src/zip_entry_new.c',
							'src/zip_err_str.c',
							'src/zip_error_get_sys_type.c',
							'src/zip_error_get.c',
							'src/zip_error_strerror.c',
							'src/zip_error_to_str.c',
							'src/zip_error.c',
							'src/zip_fclose.c',
							'src/zip_file_error_get.c',
							'src/zip_file_get_offset.c',
							'src/zip_file_strerror.c',
							'src/zip_fopen_index.c',
							'src/zip_fopen.c',
							'src/zip_fread.c',
							'src/zip_free.c',
							'src/zip_get_attributes.c',
							'src/zip_get_name.c',
							'src/zip_get_num_files.c',
							'src/zip_get_path.c',
							'src/zip_name_locate.c',
							'src/zip_new.c',
							'src/zip_open.c',
							'src/zip_recompress.c',
							'src/zip_rename.c',
							'src/zip_replace.c',
							'src/zip_set_attributes.c',
							'src/zip_set_name.c',
							'src/zip_set_progress_callback.c',
							'src/zip_source_buffer.c',
							'src/zip_source_file.c',
							'src/zip_source_filename.c',
							'src/zip_source_filep.c',
							'src/zip_source_free.c',
							'src/zip_source_function.c',
							'src/zip_stat_index.c',
							'src/zip_stat.c',
							'src/zip_strerror.c',
							'src/zip_unchange_all.c',
							'src/zip_unchange_data.c',
							'src/zip_unchange.c',
						],
						
						'direct_dependent_settings':
						{
							'include_dirs':
							[
								'include',
							],
						},
					},
					{
						'type': 'none',
						
						'direct_dependent_settings':
						{
							'libraries':
							[
								'-lzip',
							],
						},
					},
				],
			],
		},
	],
}
