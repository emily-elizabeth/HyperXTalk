{
	'variables':
	{
		'app_bundle_suffix': '',
		'ext_bundle_suffix': '.so',
		'lib_suffix': '.so',
		'ext_suffix': '.so',
		'exe_suffix': '',
		'debug_info_suffix': '.dbg',

		'c++_std': '<!(echo ${CXX_STD:-c++11})',
	},
	
	'defines':
	[
		'HAVE___THREAD',
		'_FILE_OFFSET_BITS=64',			
	],
	
	# GTK3 system headers must come before the bundled GTK2 headers so that
	# #include <gtk/gtk.h> resolves to GTK3, not the legacy GTK2 copies.
	'include_dirs':
	[
		# Filter out libpng16: GTK3 transitively pulls in /usr/include/libpng16
		# whose pnglibconf.h was built against system zlib 1.3.1, but our bundled
		# libz is 1.3.2 — causing a version-check #error in pngpriv.h.  The
		# bundled thirdparty/libpng/include (PNG_ZLIB_VERNUM=0, no check) must win.
		'<!@(pkg-config --cflags-only-I gtk+-3.0 2>/dev/null | sed "s/-I[^ ]*libpng[^ ]*//g" | sed "s/-I//g")',
		'../thirdparty/headers/linux/include',
		'../thirdparty/libcairo/src',			# Required by the GDK headers
		'../thirdparty/libfreetype/include',	# Required by the Pango headers
	],
	
	# Static libraries that are to be included into dynamic libraries
	# need to be compiled with the correct compilation flags
	'target_conditions':
	[
		[
			'server_mode == 0',
			{
				'defines':
				[
					'TARGET_PLATFORM_LINUX',
					'TARGET_PLATFORM_POSIX',
					'GTKTHEME',
					'LINUX',
					'_LINUX',
					'X11',
				],
			},
			{
				'defines':
				[
					'_LINUX',
					'_SERVER',
					'_LINUX_SERVER',
				],
			},
		],
		[
			'silence_warnings == 0',
			{
				'cflags':
				[
					'-Wall',
					'-Wextra',
#					'-Wno-deprecated-register',	# Fix when we move to C++17
					'-Wno-unused-parameter',	# Just contributes build noise
					'-Werror=return-type',
					'-Werror=uninitialized',
					'-Wno-error=maybe-uninitialized',
#					'-Werror=conversion-null',
					'-Werror=empty-body',
				],

				'cflags_cc':
				[
					'-Werror=delete-non-virtual-dtor',
					'-Werror=overloaded-virtual',
				],
			},
			{
				'cflags':
				[
					'-w',						# Disable warnings
					'-fpermissive',				# Be more lax with old code
					'-Wno-return-type',
				],
				
				'cflags_c':
				[
					'-Werror=declaration-after-statement',	# Ensure compliance with C89
				],
			},
		],
	],
	
	'cflags':
	[
		'-fPIC',
		'-fstrict-aliasing',
		'-fvisibility=hidden',
	],
	
	'cflags_c':
	[
		'-std=gnu99',
		'-Wstrict-prototypes',
	],
	
	'cflags_cc':
	[
		'-std=<(c++_std)',
		'-fno-exceptions',
		'-fno-rtti',
	],
	
	'configurations':
	{
		'Debug':
		{
			'cflags':
			[
				'-O0',
				'-g3',
			],
			
			'defines':
			[
				'_DEBUG',
			],
		},
		
		'Release':
		{
			'cflags':
			[
				'-O3',
				'-g3',
			],
			
			'defines':
			[
				'_RELEASE',
				'NDEBUG',
			],
		},
		
		'Fast':
		{
			'cflags':
			[
				'-O0',
				'-g0',
			],
			
			'defines':
			[
				'_RELEASE',
				'NDEBUG',
			],
		},
	},
}
