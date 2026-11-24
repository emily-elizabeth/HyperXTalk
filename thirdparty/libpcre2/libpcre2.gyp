{
	'includes':
	[
		'../../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'libpcre2',
			
			'toolsets': ['host', 'target'],

			'conditions':
			[
				[
					'use_system_libpcre2 == 0',
					{
						'type': 'static_library',
						
						'variables':
						{
							'silence_warnings': 1,
						},
						
						'include_dirs':
						[
							'include',
							'src',
						],
						
						'defines':
						[
							'HAVE_CONFIG_H=1',
							'PCRE2_STATIC=1',
							'PCRE2_CODE_UNIT_WIDTH=16',
						],
						
						'sources':
						[
							'include/pcre2.h',
							
							'src/config.h',
							'src/pcre2_compile.h',
							'src/pcre2_internal.h',
							'src/pcre2_intmodedep.h',
							'src/pcre2_ucp.h',
							'src/pcre2_util.h',
							
							'src/pcre2_auto_possess.c',
							'src/pcre2_chartables.c',
							'src/pcre2_chkdint.c',
							'src/pcre2_compile.c',
							'src/pcre2_compile_cgroup.c',
							'src/pcre2_compile_class.c',
							'src/pcre2_config.c',
							'src/pcre2_context.c',
							'src/pcre2_convert.c',
							'src/pcre2_dfa_match.c',
							'src/pcre2_error.c',
							'src/pcre2_extuni.c',
							'src/pcre2_find_bracket.c',
							'src/pcre2_jit_compile.c',
							'src/pcre2_maketables.c',
							'src/pcre2_match.c',
							'src/pcre2_match_data.c',
							'src/pcre2_match_next.c',
							'src/pcre2_newline.c',
							'src/pcre2_ord2utf.c',
							'src/pcre2_pattern_info.c',
							'src/pcre2_script_run.c',
							'src/pcre2_serialize.c',
							'src/pcre2_string_utils.c',
							'src/pcre2_study.c',
							'src/pcre2_substitute.c',
							'src/pcre2_substring.c',
							'src/pcre2_tables.c',
							'src/pcre2_ucd.c',
							'src/pcre2_valid_utf.c',
							'src/pcre2_xclass.c',
						],
						
						'direct_dependent_settings':
						{
							'defines':
							[
								'PCRE2_STATIC=1',
								'PCRE2_CODE_UNIT_WIDTH=16',
							],
							'include_dirs':
							[
								'include',
							],
						},
					},
					{
						'type': 'none',
						
						'link_settings':
						{
							'libraries':
							[
								'-lpcre2-16',
							],
						},
					},
				],
			],
		},
	],
}
