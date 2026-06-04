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
					'OS == "linux"',
					{
						'type': 'none',

						'direct_dependent_settings':
						{
							'include_dirs':
							[
								'/usr/include/postgresql',
							],
						},

						'link_settings':
						{
							'libraries':
							[
								'-lpq',
							],
						},
					},
				],
				[
					'OS == "win"',
					{
						'type': 'none',

						'link_settings':
						{
							'libraries':
							[
								'$(SolutionDir)$(Configuration)/lib/libpq.lib',
							],
						},

						'direct_dependent_settings':
						{
							'include_dirs':
							[
								'include',
							],
						},
					},
				],
				[
					'OS == "mac"',
					{
						'type': 'none',

						'link_settings':
						{
							'libraries':
							[
								'../../prebuilt/lib/mac/libpq.a',
							],
						},

						'direct_dependent_settings':
						{
							'include_dirs':
							[
								'include',
							],
						},
					},
				],
			],
		},
	],
}
