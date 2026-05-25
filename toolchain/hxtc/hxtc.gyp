{
    'includes':
    [
        '../../common.gypi',
    ],

    'targets':
    [
        {
            # hxtc — HyperXTalk compiled library compiler.
            #
            # Produces a standalone command-line tool that converts a
            # .livecodescript source file into a .hxtlib compiled library.
            #
            # This target intentionally has no engine dependency: it links
            # only against hxtlib (the format library).  Once the engine
            # AST serialisation layer is defined, add a dependency on the
            # engine's parse library here and update src/main.cpp.

            'target_name': 'hxtc',
            'type': 'executable',

            'include_dirs':
            [
                '../../hxtlib',
            ],

            'sources':
            [
                'src/main.cpp',
                '../../hxtlib/hxtlib.cpp',
            ],

            'conditions':
            [
                [
                    'OS == "mac"',
                    {
                        'xcode_settings':
                        {
                            'CLANG_CXX_LANGUAGE_STANDARD': 'c++17',
                            'CLANG_CXX_LIBRARY':           'libc++',
                            'MACOSX_DEPLOYMENT_TARGET':    '10.15',
                            'OTHER_CPLUSPLUSFLAGS':
                            [
                                '-Wall',
                                '-Wextra',
                            ],
                        },
                    },
                ],
                [
                    'OS == "linux"',
                    {
                        'cflags_cc':
                        [
                            '-std=c++17',
                            '-Wall',
                            '-Wextra',
                        ],
                    },
                ],
                [
                    'OS == "win"',
                    {
                        'msvs_settings':
                        {
                            'VCCLCompilerTool':
                            {
                                'AdditionalOptions': '/std:c++17 /W3',
                            },
                        },
                    },
                ],
            ],
        },
    ],
}
