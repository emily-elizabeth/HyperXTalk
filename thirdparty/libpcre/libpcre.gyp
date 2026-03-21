{
	'includes':
	[
		'../../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'libpcre',
			
			'toolsets': ['host', 'target'],

			'conditions':
			[
				[
					'use_system_libpcre == 0',
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
							'PCRE_STATIC=1',
						],
						
						'sources':
						[
							'include/pcre.h',
							'include/pcreposix.h',
							
							'src/config.h',
							'src/pcre_internal.h',
							'src/ucp.h',
							
							'src/pcre16_byte_order.c',
							'src/pcre16_chartables.c',
							'src/pcre16_compile.c',
							'src/pcre16_config.c',
							'src/pcre16_dfa_exec.c',
							'src/pcre16_exec.c',
							'src/pcre16_fullinfo.c',
							'src/pcre16_get.c',
							'src/pcre16_globals.c',
							'src/pcre16_jit_compile.c',
							'src/pcre16_maketables.c',
							'src/pcre16_newline.c',
							'src/pcre16_ord2utf16.c',
							'src/pcre16_refcount.c',
							'src/pcre16_string_utils.c',
							'src/pcre16_study.c',
							'src/pcre16_tables.c',
							'src/pcre16_ucd.c',
							'src/pcre16_valid_utf16.c',
							'src/pcre16_version.c',
							'src/pcre16_xclass.c',
						],
						
						'direct_dependent_settings':
						{
							'defines':
							[
								'PCRE_STATIC=1',
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
								'-lpcre',
							],
						},
					},
				],
			],
		},
	],
}
