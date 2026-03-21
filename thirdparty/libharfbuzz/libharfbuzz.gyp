{
	'includes':
	[
		'../../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'libharfbuzz',
			'type': 'static_library',

			'toolsets': ['target', 'host'],

			'variables':
			{
				'silence_warnings': 1,
			},
			
			'dependencies':
			[
				'../../prebuilt/libicu.gyp:libicu_include',
				'../libfreetype/libfreetype.gyp:libfreetype',
			],
			
			'defines':
			[
				'HB_NO_MT',
				'HAVE_OT',
				'HAVE_UCDN',
				'HAVE_FREETYPE',
			],
			
			'include_dirs':
			[
				'src',
				'src/hb-ucdn',
			],
			
			'sources':
			[
				'src/hb-blob.cc',
				'src/hb-buffer.cc',
				'src/hb-buffer-serialize.cc',
				'src/hb-common.cc',
				'src/hb-fallback-shape.cc',
				'src/hb-face.cc',
				'src/hb-font.cc',
				'src/hb-ft.cc',
				'src/hb-icu.cc',
				'src/hb-ot-layout.cc',
				'src/hb-ot-map.cc',
				'src/hb-ot-shape.cc',
				'src/hb-ot-shape-complex-arabic.cc',
				'src/hb-ot-shape-complex-default.cc',
				'src/hb-ot-shape-complex-hangul.cc',
				'src/hb-ot-shape-complex-hebrew.cc',
				'src/hb-ot-shape-complex-indic.cc',
				'src/hb-ot-shape-complex-indic-table.cc',
				'src/hb-ot-shape-complex-myanmar.cc',
				'src/hb-ot-shape-complex-sea.cc',
				'src/hb-ot-shape-complex-thai.cc',
				'src/hb-ot-shape-complex-tibetan.cc',
				'src/hb-ot-shape-fallback.cc',
				'src/hb-ot-shape-normalize.cc',
				'src/hb-ot-tag.cc',
				'src/hb-set.cc',
				'src/hb-shape.cc',
				'src/hb-shape-plan.cc',
				'src/hb-shaper.cc',
				'src/hb-ucdn.cc',
				'src/hb-unicode.cc',
				'src/hb-warning.cc',
				'src/hb-ucdn/ucdn.c',
			],
			
			'direct_dependent_settings':
			{
				'include_dirs':
				[
					'src',
					'../../prebuilt/include',
				],
			},

			'target_conditions':
			[
				# This library is only required on Emscripten and Android
				[
					'toolset_os not in ("emscripten", "android")',
					{
						'type': 'none',
					},
				],
			],
		},
	],
}
