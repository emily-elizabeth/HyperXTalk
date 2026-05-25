# libvlc.gyp — build configuration for libVLC 3.
#
# Headers are vendored in thirdparty/libvlc/include/.
# The actual libvlc dylib / .lib is found at:
#   macOS  — /Applications/VLC.app/Contents/MacOS/lib/libvlc.dylib
#   Linux  — system-installed libvlc (from libvlc-dev package)
#   Windows — SDK alongside the VLC installation

{
    'includes':
    [
        '../../common.gypi',
    ],

    'targets':
    [
        {
            'target_name': 'libvlc_headers',
            'type': 'none',

            'direct_dependent_settings':
            {
                'include_dirs':
                [
                    'include',
                ],
            },
        },
    ],
}
