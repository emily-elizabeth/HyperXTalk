{

    'includes':
    [
        '../../../../common.gypi',
    ],

    'targets':
    [
        {
            'target_name': 'yyjson',
            'type': 'none',

            'conditions':
            [
                [
                    'OS == "ios" and "iphoneos" in target_sdk',
                    {
                        'dependencies':
                        [
                            'yyjson-lcext',
                        ],
                    },
                    {
                        'dependencies':
                        [
                            'yyjson-build',
                        ],

                        'copies':
                        [
                            {
                                'destination': '<(PRODUCT_DIR)/packaged_extensions/com.hyperxtalk.library.json/code/<(platform_id)/',
                                'files':
                                [
                                    '<(PRODUCT_DIR)/yyjson<(SHARED_LIB_SUFFIX)',
                                ],
                            },
                        ],
                    },
                ],
            ],
        },
        {
            'target_name': 'yyjson-build',
            'product_prefix': '',
            'product_name': 'yyjson',
            'sources':
            [
                'yyjson.c',
                'yyjson.h',
                'json_bridge.c',
            ],

            'conditions':
            [
                [
                    'OS == "ios" and "iphoneos" in target_sdk',
                    {
                        'type': 'static_library',
                    },
                    'OS == "ios" and "iphoneos" not in target_sdk',
                    {
                        'type': 'shared_library',
                    },
                    {
                        'type': 'loadable_module',
                    },
                ],
            ],
        },
        {
            'target_name': 'yyjson-lcext',
            'product_prefix': '',
            'product_name': 'yyjson',
            'dependencies':
            [
                '../../../../toolchain/lc-compile/lc-compile.gyp:lc-compile',
                'yyjson-build',
            ],

            'type': 'none',

            'actions':
            [
                {
                    'action_name': 'json_output_auxc',

                    'sources':
                    [
                        '../json.lcb',
                    ],

                    'inputs':
                    [
                        '<(_sources)',
                    ],

                    'outputs':
                    [
                        '<(SHARED_INTERMEDIATE_DIR)/json.cpp',
                    ],

                    'message': 'Output module as auxc',

                    'action':
                    [
                        '>(lc-compile_host)',
                        '--forcebuiltins',
                        '--modulepath',
                        '<(PRODUCT_DIR)/modules/lci',
                        '--outputauxc',
                        '<(SHARED_INTERMEDIATE_DIR)/json.cpp',
                        '<(_sources)',
                    ],
                },
                {
                    'action_name': 'link_json_lcext',

                    'inputs':
                    [
                        '../../../../tools/build-module-lcext-ios.sh',
                        '<(SHARED_INTERMEDIATE_DIR)/json.cpp',
                        '../json.ios',
                        '<(PRODUCT_DIR)/yyjson<(STATIC_LIB_SUFFIX)',
                    ],

                    'outputs':
                    [
                        '<(PRODUCT_DIR)/yyjson.lcext',
                        '<(PRODUCT_DIR)/packaged_extensions/com.hyperxtalk.library.json/code/<(platform_id)/module.lcm',
                    ],

                    'message': 'Link lcext',

                    'action':
                    [
                        '../../../../tools/build-module-lcext-ios.sh',
                        '<(SHARED_INTERMEDIATE_DIR)/json.cpp',
                        '../json.ios',
                        '<(PRODUCT_DIR)/packaged_extensions/com.hyperxtalk.library.json/code/<(platform_id)/yyjson.lcext',
                        '$(not_a_real_variable)com.hyperxtalk.library.json',
                        '<(PRODUCT_DIR)/packaged_extensions/com.hyperxtalk.library.json/code/<(platform_id)/module.lcm',
                        '<(PRODUCT_DIR)/yyjson<(STATIC_LIB_SUFFIX)',
                    ],
                },
            ],
        },
    ],

}
