# Stub gyp file for libcef thirdparty sources.
#
# On Windows the browser external uses WebView2 (not CEF), so these targets
# are intentional no-ops.  They exist solely so that GYP can resolve the
# dependency references in revbrowser/revbrowser.gyp and
# libbrowser/libbrowser.gyp without failing on a missing file.
#
# On Linux, CEF is used for the browser external; the real build of the CEF
# library wrapper sources would be added here if/when that path is needed.
{
	'includes':
	[
		'../../common.gypi',
	],

	'targets':
	[
		{
			# CEF C++ library wrapper (links against libcef.so / libcef.dll).
			# No-op on Windows (WebView2 is used instead).
			'target_name': 'libcef_library_wrapper',
			'type': 'none',

			'toolsets': ['host', 'target'],
		},

		{
			# CEF stubs for platforms that do not ship the full CEF SDK.
			# No-op on Windows.
			'target_name': 'libcef_stubs',
			'type': 'none',

			'toolsets': ['host', 'target'],
		},
	],
}
