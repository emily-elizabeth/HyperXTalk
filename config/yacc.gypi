{
	'variables':
	{
		'conditions':
		[
			[
				'OS == "win"',
				{
					# Use win_flex.exe / win_bison.exe from the winflexbison3 Chocolatey
					# package instead of routing through invoke-unix.bat + Cygwin.
					# These are native Windows executables with the same CLI as GNU flex/bison.
					'flex': ['win_flex.exe'],
					'bison': ['win_bison.exe'],
				},
				{
					'flex': 'flex',
					'bison': 'bison',
				},
			],
		],
	},
}
