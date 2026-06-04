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
					'OS == "linux"',
					{
						'type': 'none',

						'direct_dependent_settings':
						{
							'include_dirs':
							[
								'/usr/include/mysql',
								'/usr/include/mariadb',
							],
						},

						'link_settings':
						{
							'libraries':
							[
								'-lmysqlclient',
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
								'$(SolutionDir)$(Configuration)/lib/libmysql.lib',
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
								'../../prebuilt/lib/mac/libmysql.a',
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
