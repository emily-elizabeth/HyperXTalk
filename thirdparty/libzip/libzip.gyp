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
#							'include',
							'src',
						],
						
						# libzip 1.12, built without bzip2, xz, zstd or WinZip AES
						# (see ORIGIN). config.h and zipconf.h are hand-written in
						# place of the files libzip's CMake build generates.
						'sources':
						[
							'src/zip.h',
							'src/zipconf.h',
							
							'src/compat.h',
							'src/config.h',
							'src/siphash.h',
							'src/zip_random.h',
							'src/zip_source_file.h',
							'src/zip_source_file_stdio.h',
							'src/zip_source_file_win32.h',
							'src/zipint.h',
							
							'src/siphash.c',
							'src/zip_add.c',
							'src/zip_add_dir.c',
							'src/zip_add_entry.c',
							'src/zip_algorithm_deflate.c',
							'src/zip_buffer.c',
							'src/zip_close.c',
							'src/zip_crypto.c',
							'src/zip_delete.c',
							'src/zip_dir_add.c',
							'src/zip_dirent.c',
							'src/zip_discard.c',
							'src/zip_entry.c',
							'src/zip_err_str.c',
							'src/zip_error.c',
							'src/zip_error_clear.c',
							'src/zip_error_get.c',
							'src/zip_error_get_sys_type.c',
							'src/zip_error_strerror.c',
							'src/zip_error_to_str.c',
							'src/zip_extra_field.c',
							'src/zip_extra_field_api.c',
							'src/zip_fclose.c',
							'src/zip_fdopen.c',
							'src/zip_file_add.c',
							'src/zip_file_error_clear.c',
							'src/zip_file_error_get.c',
							'src/zip_file_get_comment.c',
							'src/zip_file_get_external_attributes.c',
							'src/zip_file_get_offset.c',
							'src/zip_file_rename.c',
							'src/zip_file_replace.c',
							'src/zip_file_set_comment.c',
							'src/zip_file_set_encryption.c',
							'src/zip_file_set_external_attributes.c',
							'src/zip_file_set_mtime.c',
							'src/zip_file_strerror.c',
							'src/zip_fopen.c',
							'src/zip_fopen_encrypted.c',
							'src/zip_fopen_index.c',
							'src/zip_fopen_index_encrypted.c',
							'src/zip_fread.c',
							'src/zip_fseek.c',
							'src/zip_ftell.c',
							'src/zip_get_archive_comment.c',
							'src/zip_get_archive_flag.c',
							'src/zip_get_encryption_implementation.c',
							'src/zip_get_file_comment.c',
							'src/zip_get_name.c',
							'src/zip_get_num_entries.c',
							'src/zip_get_num_files.c',
							'src/zip_hash.c',
							'src/zip_io_util.c',
							'src/zip_libzip_version.c',
							'src/zip_memdup.c',
							'src/zip_name_locate.c',
							'src/zip_new.c',
							'src/zip_open.c',
							'src/zip_pkware.c',
							'src/zip_progress.c',
							'src/zip_realloc.c',
							'src/zip_rename.c',
							'src/zip_replace.c',
							'src/zip_set_archive_comment.c',
							'src/zip_set_archive_flag.c',
							'src/zip_set_default_password.c',
							'src/zip_set_file_comment.c',
							'src/zip_set_file_compression.c',
							'src/zip_set_name.c',
							'src/zip_source_accept_empty.c',
							'src/zip_source_at_eof.c',
							'src/zip_source_begin_write.c',
							'src/zip_source_begin_write_cloning.c',
							'src/zip_source_buffer.c',
							'src/zip_source_call.c',
							'src/zip_source_close.c',
							'src/zip_source_commit_write.c',
							'src/zip_source_compress.c',
							'src/zip_source_crc.c',
							'src/zip_source_error.c',
							'src/zip_source_file_common.c',
							'src/zip_source_file_stdio.c',
							'src/zip_source_free.c',
							'src/zip_source_function.c',
							'src/zip_source_get_dostime.c',
							'src/zip_source_get_file_attributes.c',
							'src/zip_source_is_deleted.c',
							'src/zip_source_layered.c',
							'src/zip_source_open.c',
							'src/zip_source_pass_to_lower_layer.c',
							'src/zip_source_pkware_decode.c',
							'src/zip_source_pkware_encode.c',
							'src/zip_source_read.c',
							'src/zip_source_remove.c',
							'src/zip_source_rollback_write.c',
							'src/zip_source_seek.c',
							'src/zip_source_seek_write.c',
							'src/zip_source_stat.c',
							'src/zip_source_supports.c',
							'src/zip_source_tell.c',
							'src/zip_source_tell_write.c',
							'src/zip_source_window.c',
							'src/zip_source_write.c',
							'src/zip_source_zip.c',
							'src/zip_source_zip_new.c',
							'src/zip_stat.c',
							'src/zip_stat_index.c',
							'src/zip_stat_init.c',
							'src/zip_strerror.c',
							'src/zip_string.c',
							'src/zip_unchange.c',
							'src/zip_unchange_all.c',
							'src/zip_unchange_archive.c',
							'src/zip_unchange_data.c',
							'src/zip_utf-8.c',
						],
						
						'conditions':
						[
							[
								'OS == "win"',
								{
									# Win32 file I/O (UTF-8 paths become UTF-16) and
									# CryptGenRandom, which seeds the key of every
									# archive's name table and makes PKWARE
									# encryption headers. Temporary file names come
									# from GetTickCount.
									'sources':
									[
										'src/zip_random_win32.c',
										'src/zip_source_file_win32.c',
										'src/zip_source_file_win32_ansi.c',
										'src/zip_source_file_win32_named.c',
										'src/zip_source_file_win32_utf16.c',
										'src/zip_source_file_win32_utf8.c',
									],
									
									'defines':
									[
										'_CRT_SECURE_NO_WARNINGS',
										'_CRT_NONSTDC_NO_DEPRECATE',
									],
									
									'link_settings':
									{
										'libraries':
										[
											'-ladvapi32',
										],
									},
								},
								{
									'sources':
									[
										'src/zip_random_unix.c',
										'src/zip_source_file_stdio_named.c',
									],
								},
							],
						],
						
						'direct_dependent_settings':
						{
							'include_dirs':
							[
#								'include',
								'src',
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
