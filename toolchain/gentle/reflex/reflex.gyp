{
	'includes':
	[
		'../../../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'reflex',
			'type': 'executable',
			
			'toolsets': ['host','target'],
			
			'product_name': 'reflex-<(_toolset)',
		
			'variables':
			{
				'silence_warnings': 1,
			},

			# reflex.c is ancient K&R C - force C89 to allow implicit int.
			'xcode_settings':
			{
				'GCC_C_LANGUAGE_STANDARD': 'c89',
			},
			'cflags': [ '-std=gnu89' ],

			'direct_dependent_settings':
			{
				'variables':
				{
					'reflex_exe_file': '<(PRODUCT_DIR)/<(_product_name)<(EXECUTABLE_SUFFIX)',
				},
			},
			
			'sources':
			[
				'reflex.c',
			],
			
			'msvs_settings':
			{
				'VCLinkerTool':
				{
					'SubSystem': '1',	# /SUBSYSTEM:CONSOLE
				},
			},
		},
	],
}

