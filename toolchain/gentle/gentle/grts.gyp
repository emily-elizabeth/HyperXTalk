{
	'includes':
	[
		'../../../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'grts',
			'type': 'static_library',
			
			'toolsets': ['host','target'],
			
			'product_name': 'grts',
		
			'variables':
			{
				'silence_warnings': 1,
			},

			# grts.c is ancient K&R C that uses implicit int.
			# Modern Clang treats -Wimplicit-int as an error regardless of -w,
			# so force C89 mode which permits it.
			'xcode_settings':
			{
				'GCC_C_LANGUAGE_STANDARD': 'c89',
			},
			'cflags': [ '-std=gnu89' ],

			'sources':
			[
				'grts.c',
			],
			
			'target_conditions':
			[
				[
					'_toolset != "target"',
					{
						'product_name': 'grts->(_toolset)',
					},
				],
			],
		},
	],
}
