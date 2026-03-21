{
	'includes':
	[
		'../../common.gypi',
	],
	
	'variables':
	{
		'skia_include_dirs':
		[
			'include/android',
			'include/animator',
			'include/c',
			'include/codec',
			'include/config',
			'include/core',
			'include/effects',
			'include/images',
			'include/pathops',
			'include/ports',
			'include/private',
			'include/svg',
			'include/utils',
            'include/utils/ios',
            'include/utils/mac',
            'include/utils/win',
			'include/views',
			'include/xml',
			'src/android',
			'src/animator',
			'src/c',
			'src/codec',
			'src/core',
			'src/effects',
			'src/fonts',
			'src/image',
			'src/images',
			'src/lazy',
			'src/opts',
			'src/pathops',
			'src/pdf',
			'src/pipe',
			'src/ports',
			'src/sfnt',
			'src/svg',
			'src/utils',
			'src/views',
			'src/xml',
			'src/xps',
		],

		'skia_defines':
		[
			# Disable Skia debugging
			'SK_RELEASE',
			
			# Don't try to link against libetc1 for ETC1 texture compression support
			'SK_IGNORE_ETC1_SUPPORT',
			
			# Some Skia source files need this to build
			# TODO: see if those files can be removed from the build
			'SK_SUPPORT_LEGACY_IMAGE_ENCODER_CLASS',
			
			# We use deprecated Skia features
			'SK_SUPPORT_LEGACY_CANVAS_IS_REFCNT',
			'SK_SUPPORT_LEGACY_GETTOPDEVICE',
			'SK_SUPPORT_LEGACY_ACCESSBITMAP',
			'SK_SUPPORT_LEGACY_CLIP_REGIONOPS',
			'SK_SUPPORT_LEGACY_GETDEVICE',
            
            # Disable GPU support
            'SK_SUPPORT_GPU=0',
		],

		'opts_none_srcs':
		[
  			"src/opts/SkBitmapProcState_opts_none.cpp",
  			"src/opts/SkBlitMask_opts_none.cpp",
  			"src/opts/SkBlitRow_opts_none.cpp",
		],

        'opts_arm_srcs':
        [
            "src/opts/SkBitmapProcState_opts_none.cpp",
			"src/opts/SkBlitMask_opts_arm.cpp",
			"src/opts/SkBlitRow_opts_arm.cpp",
        ],
        
		'opts_armv7_arm64_srcs':
		[
			"src/opts/SkBitmapProcState_arm_neon.cpp",
			"src/opts/SkBitmapProcState_matrixProcs_neon.cpp",
			"src/opts/SkBlitMask_opts_arm_neon.cpp",
			"src/opts/SkBlitRow_opts_arm_neon.cpp",
		],

		'opts_crc32_srcs':
		[
			"src/opts/SkOpts_crc32.cpp",
		],

		'opts_sse2_srcs':
		[
			"src/opts/SkBitmapProcState_opts_SSE2.cpp",
			"src/opts/SkBlitRow_opts_SSE2.cpp",
			"src/opts/opts_check_x86.cpp",
		],

		'opts_sse3_srcs':
		[ 
			"src/opts/SkBitmapProcState_opts_SSSE3.cpp",
  			"src/opts/SkOpts_ssse3.cpp",
  		],

  		'opts_sse41_srcs':
  		[
  			"src/opts/SkOpts_sse41.cpp",
  		],

  		'opts_sse42_srcs':
  		[
  			"src/opts/SkOpts_sse42.cpp",
  		],

  		'opts_avx_srcs':
  		[
  			"src/opts/SkOpts_avx.cpp",
  		],

  		'opts_hsw_srcs':
  		[
  			"src/opts/SkOpts_hsw.cpp",
  		],
	},

	'target_defaults':
	{
		'target_conditions':
		[
			[
				'toolset_os == "android" and toolset_arch == "armv6"',
				{
					'cflags':
					[
						# Skia won't compile to ARMv6 Thumb
						'-marm',
					],
				},
			],
		],
	},

	'targets':
	[
		# We define separate targets for each set of optimizations as they need
		# specific compiler flags which must not use generally.
		#
		# Each opt target contains 'opts_dummy.cpp' in the sources to ensure
		# that we don't get no .a file for them, making it easier to include
		# in the main skia target.

		{
			'target_name': 'libskia_opt_none',
			'type': 'static_library',

			'toolsets': ['host', 'target'],

			'include_dirs':
			[
				'<@(skia_include_dirs)',
			],

			'defines':
			[
				'<@(skia_defines)',
			],

			'variables':
			{
				'silence_warnings': 1,
			},

			'sources':
			[
				'src/opts/opts_dummy.cpp',
			],

			'target_conditions':
			[
				[
					'toolset_arch not in ("i386", "x86", "x86_64", "i386 x86_64", "x64", "armv6", "armv6hf", "armv7", "arm64", "armv7 arm64")',
					{
						'sources':
						[
							'<@(opts_none_srcs)',
						],
					},
				],
			],
		},

		{
			'target_name': 'libskia_opt_arm',
			'type': 'static_library',

			'toolsets': ['host', 'target'],

			'include_dirs':
			[
				'<@(skia_include_dirs)',
			],

			'defines':
			[
				'<@(skia_defines)',
			],

			'variables':
			{
				'silence_warnings': 1,
			},

			'sources':
			[
				'src/opts/opts_dummy.cpp',
			],

			'target_conditions':
			[
				[
					'toolset_arch in ("armv7", "arm64", "armv7 arm64")',
					{
						'sources':
						[
                            '<@(opts_arm_srcs)',
                            '<@(opts_armv7_arm64_srcs)',
                            '<@(opts_crc32_srcs)',
						],

						'target_conditions':
						[
							[
								'toolset_os == "android" and toolset_arch == "armv7"',
								{
									'cflags':
									[
										# Needed in order to enable NEON instruction support
										'-mfpu=neon',
									],
								},
							],
						],
					},
				],
                [
                    'toolset_arch in ("armv6", "armv6hf")',
                    {
                        'sources':
                        [
                            '<@(opts_arm_srcs)',
                            '<@(opts_crc32_srcs)',
                        ],
                    },
                ],
			],
		},

		{
			'target_name': 'libskia_opt_sse2',
			'type': 'static_library',

			'toolsets': ['host', 'target'],

			'include_dirs':
			[
				'<@(skia_include_dirs)',
			],

			'defines':
			[
				'<@(skia_defines)',
			],
			
			'variables':
			{
				'silence_warnings': 1,
			},

			'sources':
			[
				'src/opts/opts_dummy.cpp',
			],

			'target_conditions':
			[
				[
					'toolset_arch in ("i386", "x86", "x64", "x86_64", "i386 x86_64")',
					{
						'sources':
						[
							'<@(opts_sse2_srcs)',
						],
                        
                        'target_conditions':
                        [
                            [
                                'toolset_os == "win"',
                                {
                                    'defines':
                                    [
                                        'SK_CPU_SSE_LEVEL=SK_CPU_SSE_LEVEL_SSE2',
                                    ],
                                },
                                {
                                    'cflags':
                                    [
                                        '-msse2',
                                    ],
                                },
                            ],
                        ],
					},
				],
			],
		},

		{
			'target_name': 'libskia_opt_sse3',
			'type': 'static_library',

			'toolsets': ['host', 'target'],

			'include_dirs':
			[
				'<@(skia_include_dirs)',
			],

			'defines':
			[
				'<@(skia_defines)',
			],

			'variables':
			{
				'silence_warnings': 1,
			},

			'sources':
			[
				'src/opts/opts_dummy.cpp',
			],

			'target_conditions':
			[
				[
					'toolset_arch in ("i386", "x86", "x64", "x86_64", "i386 x86_64")',
					{
						'sources':
						[
							'<@(opts_sse3_srcs)',
						],
                        
                        'target_conditions':
                        [
                            [
                                'toolset_os == "win"',
                                {
                                    'defines':
                                    [
                                        'SK_CPU_SSE_LEVEL=SK_CPU_SSE_LEVEL_SSE3',
                                    ],
                                },
                                {
                                    'cflags':
                                    [
                                        '-msse3',
                                    ],
                                },
                            ],
                        ],
					},
				],
			],
		},

		{
			'target_name': 'libskia_opt_sse41',
			'type': 'static_library',

			'toolsets': ['host', 'target'],

			'include_dirs':
			[
				'<@(skia_include_dirs)',
			],

			'defines':
			[
				'<@(skia_defines)',
			],
			
			'variables':
			{
				'silence_warnings': 1,
			},

			'sources':
			[
				'src/opts/opts_dummy.cpp',
			],

			'target_conditions':
			[
				[
					'toolset_arch in ("i386", "x86", "x64", "x86_64", "i386 x86_64")',
					{
						'sources':
						[
							'<@(opts_sse41_srcs)',
						],
                        
                        'target_conditions':
                        [
                            [
                                'toolset_os == "win"',
                                {
                                    'defines':
                                    [
                                        'SK_CPU_SSE_LEVEL=SK_CPU_SSE_LEVEL_SSE41',
                                    ],
                                },
                                {
                                    'cflags':
                                    [
                                        '-msse4.1',
                                    ],
                                },
                            ],
                        ],
					},
				],
			],
		},


		{
			'target_name': 'libskia_opt_sse42',
			'type': 'static_library',

			'toolsets': ['host', 'target'],

			'include_dirs':
			[
				'<@(skia_include_dirs)',
			],

			'defines':
			[
				'<@(skia_defines)',
			],
			
			'variables':
			{
				'silence_warnings': 1,
			},

			'sources':
			[
				'src/opts/opts_dummy.cpp',
			],

			'target_conditions':
			[
				[
					'toolset_arch in ("i386", "x86", "x64", "x86_64", "i386 x86_64")',
					{
						'sources':
						[
							'<@(opts_sse42_srcs)',
						],
                        
                        'target_conditions':
                        [
                            [
                                'toolset_os == "win"',
                                {
                                    'defines':
                                    [
                                        'SK_CPU_SSE_LEVEL=SK_CPU_SSE_LEVEL_SSE42',
                                    ],
                                },
                                {
                                    'cflags':
                                    [
                                        '-msse4.2',
                                    ],
                                },
                            ],
                        ],
					},
				],
			],
		},

		{
			'target_name': 'libskia_opt_avx',
			'type': 'static_library',

			'toolsets': ['host', 'target'],

			'include_dirs':
			[
				'<@(skia_include_dirs)',
			],

			'defines':
			[
				'<@(skia_defines)',
			],

			'variables':
			{
				'silence_warnings': 1,
			},

			'sources':
			[
				'src/opts/opts_dummy.cpp',
			],

			'target_conditions':
			[
				[
					'toolset_arch in ("i386", "x86", "x64", "x86_64", "i386 x86_64")',
					{
						'sources':
						[
							'<@(opts_avx_srcs)',
						],
                        
                        'target_conditions':
                        [
                            [
                                'toolset_os == "win"',
                                {
                                    'cflags':
                                    [
                                        '/arch:AVX',
                                    ],
                                },
                                {
                                    'cflags':
                                    [
                                        '-mavx',
                                    ],
                                },
                            ],
                        ],
					},
				],
			],
		},

		{
			'target_name': 'libskia_opt_hsw',
			'type': 'static_library',

			'toolsets': ['host', 'target'],

			'include_dirs':
			[
				'<@(skia_include_dirs)',
			],

			'defines':
			[
				'<@(skia_defines)',
			],
			
			'variables':
			{
				'silence_warnings': 1,
			},

			'sources':
			[
				'src/opts/opts_dummy.cpp',
			],

			'target_conditions':
			[
				[
					'toolset_arch in ("i386", "x86", "x64", "x86_64", "i386 x86_64")',
					{
						'sources':
						[
							'<@(opts_hsw_srcs)',
						],
                        
                        'target_conditions':
                        [
                            [
                                'toolset_os == "win"',
                                {
                                    'cflags':
                                    [
                                        '/arch:AVX2',
                                    ],
                                },
                                {
                                    'cflags':
                                    [
                                        '-mavx2',
                                        '-mbmi',
                                        '-mbmi2',
                                        '-mf16c',
                                        '-mfma',
                                    ],
                                },
                            ],
                        ],
					},
				],
			],
		},

		{
			'target_name': 'libskia',
			'type': 'static_library',
			
			'toolsets': ['host', 'target'],

			'variables':
			{
				'silence_warnings': 1,
			},
			
			'dependencies':
			[
				'../libgif/libgif.gyp:libgif',
				'../libexpat/libexpat.gyp:libexpat',
				'../libjpeg/libjpeg.gyp:libjpeg',
				'../libpng/libpng.gyp:libpng',
				'../libz/libz.gyp:libz',

				'libskia_opt_none',
				'libskia_opt_arm',
				'libskia_opt_sse2',
				'libskia_opt_sse3',
				'libskia_opt_sse41',
				'libskia_opt_sse42',
				'libskia_opt_avx',
				'libskia_opt_hsw',
			],
			
			'include_dirs':
			[
				'<@(skia_include_dirs)',
			],
			
			'sources':
			[
                #'src/android/SkBitmapRegionCodec.h',
                #'src/android/SkBitmapRegionDecoderPriv.h',
                #'src/android/SkBitmapRegionCodec.cpp',
                #'src/android/SkBitmapRegionDecoder.cpp',
				
				'src/animator/SkADrawable.h',
				'src/animator/SkAnimate.h',
				'src/animator/SkAnimateActive.h',
				'src/animator/SkAnimateBase.h',
				'src/animator/SkAnimateMaker.h',
				'src/animator/SkAnimateProperties.h',
				'src/animator/SkAnimateSet.h',
				'src/animator/SkAnimatorScript.h',
				'src/animator/SkAnimatorScript2.h',
				'src/animator/SkBoundable.h',
				'src/animator/SkDisplayable.h',
				'src/animator/SkDisplayAdd.h',
				'src/animator/SkDisplayApply.h',
				'src/animator/SkDisplayBounds.h',
				'src/animator/SkDisplayEvent.h',
				'src/animator/SkDisplayInclude.h',
				'src/animator/SkDisplayInput.h',
				'src/animator/SkDisplayList.h',
				'src/animator/SkDisplayMath.h',
				'src/animator/SkDisplayMovie.h',
				'src/animator/SkDisplayPost.h',
				'src/animator/SkDisplayRandom.h',
				'src/animator/SkDisplayScreenplay.h',
				'src/animator/SkDisplayType.h',
				'src/animator/SkDisplayTypes.h',
				'src/animator/SkDisplayXMLParser.h',
				'src/animator/SkDraw3D.h',
				'src/animator/SkDrawBitmap.h',
				'src/animator/SkDrawBlur.h',
				'src/animator/SkDrawClip.h',
				'src/animator/SkDrawColor.h',
				'src/animator/SkDrawDash.h',
				'src/animator/SkDrawDiscrete.h',
				'src/animator/SkDrawEmboss.h',
				'src/animator/SkDrawExtraPathEffect.h',
				'src/animator/SkDrawFull.h',
				'src/animator/SkDrawGradient.h',
				'src/animator/SkDrawGroup.h',
				'src/animator/SkDrawLine.h',
				'src/animator/SkDrawMatrix.h',
				'src/animator/SkDrawOval.h',
				'src/animator/SkDrawPaint.h',
				'src/animator/SkDrawPath.h',
				'src/animator/SkDrawPoint.h',
				'src/animator/SkDrawRectangle.h',
				'src/animator/SkDrawSaveLayer.h',
				'src/animator/SkDrawShader.h',
				'src/animator/SkDrawText.h',
				'src/animator/SkDrawTextBox.h',
				'src/animator/SkDrawTo.h',
				'src/animator/SkDump.h',
				'src/animator/SkExtras.h',
				'src/animator/SkHitClear.h',
				'src/animator/SkIntArray.h',
				'src/animator/SkMatrixParts.h',
				'src/animator/SkMemberInfo.h',
				'src/animator/SkOpArray.h',
				'src/animator/SkOperand.h',
				'src/animator/SkOperand2.h',
				'src/animator/SkOperandInterpolator.h',
				'src/animator/SkPaintPart.h',
				'src/animator/SkPathParts.h',
				'src/animator/SkPostParts.h',
				'src/animator/SkScript.h',
				'src/animator/SkScript2.h',
				'src/animator/SkScriptCallback.h',
				'src/animator/SkScriptRuntime.h',
				'src/animator/SkSnapshot.h',
				'src/animator/SkTDArray_Experimental.h',
				'src/animator/SkTDStack.h',
				'src/animator/SkTextOnPath.h',
				'src/animator/SkTextToPath.h',
				'src/animator/SkTypedArray.h',
				'src/animator/SkXMLAnimatorWriter.h',
				'src/animator/SkADrawable.cpp',
				'src/animator/SkAnimateActive.cpp',
				'src/animator/SkAnimateBase.cpp',
				'src/animator/SkAnimateField.cpp',
				'src/animator/SkAnimateMaker.cpp',
				'src/animator/SkAnimateSet.cpp',
				'src/animator/SkAnimator.cpp',
				'src/animator/SkAnimatorScript.cpp',
				'src/animator/SkAnimatorScript2.cpp',
				'src/animator/SkBoundable.cpp',
				'src/animator/SkBuildCondensedInfo.cpp',
				'src/animator/SkDisplayable.cpp',
				'src/animator/SkDisplayAdd.cpp',
				'src/animator/SkDisplayBounds.cpp',
				'src/animator/SkDisplayEvent.cpp',
				'src/animator/SkDisplayEvents.cpp',
				'src/animator/SkDisplayInclude.cpp',
				'src/animator/SkDisplayInput.cpp',
				'src/animator/SkDisplayList.cpp',
				'src/animator/SkDisplayMath.cpp',
				'src/animator/SkDisplayMovie.cpp',
				'src/animator/SkDisplayNumber.cpp',
				'src/animator/SkDisplayPost.cpp',
				'src/animator/SkDisplayRandom.cpp',
				'src/animator/SkDisplayScreenplay.cpp',
				'src/animator/SkDisplayType.cpp',
				'src/animator/SkDisplayTypes.cpp',
				'src/animator/SkDisplayXMLParser.cpp',
				'src/animator/SkDraw3D.h',
				'src/animator/SkDrawBitmap.cpp',
				'src/animator/SkDrawBlur.cpp',
				'src/animator/SkDrawClip.cpp',
				'src/animator/SkDrawColor.cpp',
				'src/animator/SkDrawDash.cpp',
				'src/animator/SkDrawDiscrete.cpp',
				'src/animator/SkDrawEmboss.cpp',
				'src/animator/SkDrawExtraPathEffect.cpp',
				'src/animator/SkDrawFull.cpp',
				'src/animator/SkDrawGradient.cpp',
				'src/animator/SkDrawGroup.cpp',
				'src/animator/SkDrawLine.cpp',
				'src/animator/SkDrawMatrix.cpp',
				'src/animator/SkDrawOval.cpp',
				'src/animator/SkDrawPaint.cpp',
				'src/animator/SkDrawPath.cpp',
				'src/animator/SkDrawPoint.cpp',
				'src/animator/SkDrawRectangle.cpp',
				'src/animator/SkDrawSaveLayer.cpp',
				'src/animator/SkDrawShader.cpp',
				'src/animator/SkDrawText.cpp',
				'src/animator/SkDrawTextBox.cpp',
				'src/animator/SkDrawTo.cpp',
				'src/animator/SkDump.cpp',
				'src/animator/SkGetCondensedInfo.cpp',
				'src/animator/SkHitClear.cpp',
				'src/animator/SkHitTest.cpp',
				'src/animator/SkMatrixParts.cpp',
				'src/animator/SkMemberInfo.cpp',
				'src/animator/SkOpArray.cpp',
				'src/animator/SkOperandIterpolator.cpp',
				'src/animator/SkPaintPart.h',
				'src/animator/SkParseSVGPath.cpp',
				'src/animator/SkPathParts.cpp',
				'src/animator/SkPostParts.cpp',
				'src/animator/SkScript.cpp',
				'src/animator/SkScriptDecompile.cpp',
				'src/animator/SkScriptRuntime.cpp',
				'src/animator/SkScriptTokenizer.cpp',
				'src/animator/SkSnapshot.cpp',
				'src/animator/SkTextOnPath.cpp',
				'src/animator/SkTextToPath.cpp',
				'src/animator/SkTypedArray.cpp',
				'src/animator/SkXMLAnimatorWriter.cpp',
				
				'src/c/sk_c_from_to.h',
				'src/c/sk_types_priv.h',
				'src/c/sk_paint.cpp',
				'src/c/sk_surface.cpp',
				
				'src/codec/SkBmpCodec.h',
				'src/codec/SkBmpMaskCodec.h',
				'src/codec/SkBmpRLECodec.h',
				'src/codec/SkBmpStandardCodec.h',
				'src/codec/SkCodecAnimation.h',
				'src/codec/SkCodecImageGenerator.h',
				'src/codec/SkCodecPriv.h',
				'src/codec/SkGifCodec.h',
				'src/codec/SkIcoCodec.h',
				'src/codec/SkJpegCodec.h',
				'src/codec/SkMasks.h',
				'src/codec/SkMaskSwizzler.h',
				'src/codec/SkPngCodec.h',
				'src/codec/SkRawAdapterCodec.h',
				'src/codec/SkRawCodec.h',
				'src/codec/SkSampledCodec.h',
				'src/codec/SKSampler.h',
				'src/codec/SkStreamBuffer.h',
				'src/codec/SkSwizzler.h',
				'src/codec/SkWbmpCodec.h',
				'src/codec/SkWebpAdapterCodec.h',
				'src/codec/SkWebpCodec.h',
                'src/codec/SkAndroidCodec.cpp',
                'src/codec/SkBmpCodec.cpp',
                'src/codec/SkBmpMaskCodec.cpp',
                'src/codec/SkBmpRLECodec.cpp',
                'src/codec/SkBmpStandardCodec.cpp',
                'src/codec/SkCodec.cpp',
                'src/codec/SkCodecImageGenerator.cpp',
                'src/codec/SkGifCodec.cpp',
                'src/codec/SkIcoCodec.cpp',
                'src/codec/SkJpegCodec.cpp',
                'src/codec/SkJpegDecoderMgr.cpp',
                'src/codec/SkJpegUtility.cpp',
                'src/codec/SkMasks.cpp',
                'src/codec/SkMaskSwizzler.cpp',
                'src/codec/SkPngCodec.cpp',
                'src/codec/SkRawAdapterCodec.cpp',
                'src/codec/SkRawCodec.cpp',
                'src/codec/SkSampledCodec.cpp',
                'src/codec/SkSampler.cpp',
                'src/codec/SkStreamBuffer.cpp',
                'src/codec/SkSwizzler.cpp',
                'src/codec/SkWbmpCodec.cpp',
                'src/codec/SkWebpAdapterCodec.cpp',
                'src/codec/SkWebpCodec.cpp',
				
				'src/core/Sk4px.h',
				'src/core/Sk4x4f.h',
				'src/core/SkAAClip.h',
				'src/core/SkAdvancedTypefaceMetrics.h',
				'src/core/SkAnalyticEdge.h',
				'src/core/SkAnnotationKeys.h',
				'src/core/SkAntiRun.h',
				'src/core/SkATrace.h',
				'src/core/SKAutoKern.h',
				'src/core/SkAutoPixmapStorage.h',
				'src/core/SkBBoxHierarchy.h',
				'src/core/SkBigPicture.h',
				'src/core/SkBitmapCache.h',
				'src/core/SkBitmapController.h',
				'src/core/SkBitmapFilter.h',
				'src/core/SkBitmapProcShader.h',
				'src/core/SkBitmapProcState.h',
				'src/core/SkBitmapProcState_filter.h',
				'src/core/SkBitmapProcState_matrix.h',
				'src/core/SkBitmapProcState_matrix_template.h',
				'src/core/SkBitmapProcState_procs.h',
				'src/core/SkBitmapProcState_sample.h',
				'src/core/SkBitmapProcState_shaderproc.h',
				'src/core/SkBitmapProcState_utils.h',
				'src/core/SkBitmapProvider.h',
				'src/core/SkBitmapScaler.h',
				'src/core/SkBlendModePriv.h',
				'src/core/SkBlitBWMaskTemplate.h',
				'src/core/SkBlitMask.h',
				'src/core/SkBlitter.h',
				'src/core/SkBuffer.h',
				'src/core/SkCachedData.h',
				'src/core/SkCanvasPriv.h',
				'src/core/SkColorFilterShader.h',
				'src/core/SkColorLookUpTable.h',
				'src/core/SkColorMatrixFilterRowMajor255.h',
				'src/core/SkColorShader.h',
				'src/core/SkColorSpace_A2B.h',
				'src/core/SkColorSpace_Base.h',
				'src/core/SkColorSpace_XYZ.h',
				'src/core/SkColorSpacePriv.h',
				'src/core/SkColorSpaceXform_A2B.h',
				'src/core/SkColorSpaceXform_Base.h',
				'src/core/SkColorSpaceXformPriv.h',
				'src/core/SkComposeShader.h',
				'src/core/SkConfig8888.h',
				'src/core/SkConvolver.h',
				'src/core/SkCoreBlitters.h',
				'src/core/SkCpu.h',
				'src/core/SkCubicClipper.h',
				'src/core/SkDeduper.h',
				'src/core/SkDescriptor.h',
				'src/core/SkDeviceLooper.h',
				'src/core/SkDeviceProfile.h',
				'src/core/SkDiscardableMemory.h',
				'src/core/SkDistanceFieldGen.h',
				'src/core/SkDither.h',
				'src/core/SkDrawProcs.h',
				'src/core/SkEdge.h',
				'src/core/SkEdgeBuilder.h',
				'src/core/SkEmptyShader.h',
				'src/core/SkEndian.h',
				'src/core/SkFDot6.h',
				'src/core/SkFDot6Constants.h',
				'src/core/SkFilterProc.h',
				'src/core/SkFindAndPlaceGlyph.h',
				'src/core/SkFixed15.h',
				'src/core/SkFixedAlloc.h',
				'src/core/SkFontDescriptor.h',
				'src/core/SkFontStream.h',
				'src/core/SkFuzzLogging.h',
				'src/core/SkGeometry.h',
				'src/core/SkGlyph.h',
				'src/core/SkGlyphCache.h',
				'src/core/SkGlyphCache_Globals.h',
				'src/core/SkHalf.h',
				'src/core/SkImageCacherator.h',
				'src/core/SkImageFilterCache.h',
				'src/core/SkImagePriv.h',
				'src/core/SkLatticeIter.h',
				'src/core/SkLightingShader.h',
				'src/core/SkLinearBitmapPipeline.h',
				'src/core/SkLinearBitmapPipeline_core.h',
				'src/core/SkLinearBitmapPipeline_matrix.h',
				'src/core/SkLinearBitmapPipeline_sample.h',
				'src/core/SkLinearBitmapPipeline_tile.h',
				'src/core/SkLineClipper.h',
				'src/core/SkLiteDL.h',
				'src/core/SkLiteRecorder.h',
				'src/core/SkLocalMatrixImageFilter.h',
				'src/core/SkLocalMatrixShader.h',
				'src/core/SkMakeUnique.h',
				'src/core/SkMaskCache.h',
				'src/core/SkMaskGamma.h',
				'src/core/SkMathPriv.h',
				'src/core/SkMatrixImageFilter.h',
				'src/core/SkMatrixPriv.h',
				'src/core/SkMatrixUtils.h',
				'src/core/SkMD5.h',
				'src/core/SkMessageBus.h',
				'src/core/SkMipMap.h',
				'src/core/SkModeColorFilter.h',
				'src/core/SkMSAN.h',
				'src/core/SkNextID.h',
				'src/core/SkNormalBevelSource.h',
				'src/core/SkNormalFlatSource.h',
				'src/core/SkNormalMapSource.h',
				'src/core/SkNormalSource.h',
				'src/core/SkNormalSourcePriv.h',
				'src/core/SkNx.h',
				'src/core/SkOpts.h',
				'src/core/SkOrderedReadBuffer.h',
				'src/core/SkPaintDefaults.h',
				'src/core/SkPaintPriv.h',
				'src/core/SkPathMeasurePriv.h',
				'src/core/SkPathPriv.h',
				'src/core/SkPerspIter.h',
				'src/core/SkPictureCommon.h',
				'src/core/SkPictureContentInfo.h',
				'src/core/SkPictureData.h',
				'src/core/SkPictureFlat.h',
				'src/core/SkPicturePlayback.h',
				'src/core/SkPictureRecord.h',
				'src/core/SkPictureShader.h',
				'src/core/SkPipe.h',
				'src/core/SkPM4f.h',
				'src/core/SkPM4fPriv.h',
				'src/core/SkPtrRecorder.h',
				'src/core/SkQuadClipper.h',
				'src/core/SkRadialShadowMapShader.h',
				'src/core/SkRasterClip.h',
				'src/core/SkRasterPipeline.h',
				'src/core/SkReadBuffer.h',
				'src/core/SkReader32.h',
				'src/core/SkRecord.h',
				'src/core/SkRecordDraw.h',
				'src/core/SkRecordedDrawable.h',
				'src/core/SkRecorder.h',
				'src/core/SkRefDict.h',
				'src/core/SkRegionPriv.h',
				'src/core/SkResourceCache.h',
				'src/core/SkRTree.h',
				'src/core/SkScalerContext.h',
				'src/core/SkScaleToSides.h',
				'src/core/SkScan.h',
				'src/core/SkScanPriv.h',
				'src/core/SkShadowShader.h',
				'src/core/SkSharedMutex.h',
				'src/core/SkSinglyLinkedList.h',
				'src/core/SkSmallAllocator.h',
				'src/core/SkSpanProcs.h',
				'src/core/SkSpecialImage.h',
				'src/core/SkSpecialSurface.h',
				'src/core/SkSpriteBlitter.h',
				'src/core/SkSpriteBlitterTemplate.h',
				'src/core/SkSRGB.h',
				'src/core/SkStreamPriv.h',
				'src/core/SkStringUtils.h',
				'src/core/SkStroke.h',
				'src/core/SkStrokerPriv.h',
				'src/core/SkSurfacePriv.h',
				'src/core/SkTaskGroup.h',
				'src/core/SkTDPQueue.h',
				'src/core/SkTDynamicHash.h',
				'src/core/SkTextBlobRunIterator.h',
				'src/core/SkTextFormatParams.h',
				'src/core/SkTextMapStateProc.h',
				'src/core/SkTextToPathIter.h',
				'src/core/SkTInternalLList.h',
				'src/core/SkTLList.h',
				'src/core/SkTLS.h',
				'src/core/SkTMultiMap.h',
				'src/core/SkTraceEvent.h',
				'src/core/SkTraceEventCommon.h',
				'src/core/SkTSort.h',
				'src/core/SkTTopoSort.h',
				'src/core/SkTypefaceCache.h',
				'src/core/SkTypefacePriv.h',
				'src/core/SkUtils.h',
				'src/core/SkUtilsArm.h',
				'src/core/SkValidatingReadBuffer.h',
				'src/core/SkValidationUtils.h',
				'src/core/SkVarAlloc.h',
				'src/core/SkVertState.h',
				'src/core/SkXfermode_proccoeff.h',
				'src/core/SkXfermodeInterpretation.h',
				'src/core/SkXfermodePriv.h',
				'src/core/SkYUVPlanesCache.h',
				'src/core/SkAAClip.cpp',
				'src/core/SkAlphaRuns.cpp',
				'src/core/SkAnalyticEdge.cpp',
				'src/core/SkAnnotation.cpp',
				'src/core/SkATrace.cpp',
				'src/core/SkAutoPixmapStorage.cpp',
				'src/core/SkBBHFactory.cpp',
				'src/core/SkBigPicture.cpp',
				'src/core/SkBitmap.cpp',
				'src/core/SkBitmapCache.cpp',
				'src/core/SkBitmapController.cpp',
				'src/core/SkBitmapDevice.cpp',
				'src/core/SkBitmapProcShader.cpp',
				'src/core/SkBitmapProcState.cpp',
				'src/core/SkBitmapProcState_matrixProcs.cpp',
				'src/core/SkBitmapProvider.cpp',
				'src/core/SkBitmapScaler.cpp',
				'src/core/SkBlitMask_D32.cpp',
				'src/core/SkBlitRow_D16.cpp',
				'src/core/SkBlitRow_D32.cpp',
				'src/core/SkBlitter.cpp',
				'src/core/SkBlitter_A8.cpp',
				'src/core/SkBlitter_ARGB32.cpp',
				'src/core/SkBlitter_PM4f.cpp',
				'src/core/SkBlitter_RGB16.cpp',
				'src/core/SkBlitter_Sprite.cpp',
				'src/core/SkBlurImageFilter.cpp',
				'src/core/SkBuffer.cpp',
				'src/core/SkCachedData.cpp',
				'src/core/SkCanvas.cpp',
				'src/core/SkChunkAlloc.cpp',
				'src/core/SkClipStack.cpp',
				'src/core/SkColor.cpp',
				'src/core/SkColorFilter.cpp',
				'src/core/SkColorFilterShader.cpp',
				'src/core/SkColorLookUpTable.cpp',
				'src/core/SkColorMatrixFilterRowMajor255.cpp',
				'src/core/SkColorShader.cpp',
				'src/core/SkColorSpace.cpp',
				'src/core/SkColorSpace_A2B.cpp',
				'src/core/SkColorSpace_ICC.cpp',
				'src/core/SkColorSpace_XYZ.cpp',
				'src/core/SkColorSpaceXform.cpp',
				'src/core/SkColorSpaceXform_A2B.cpp',
				'src/core/SkColorTable.cpp',
				'src/core/SkComposeShader.cpp',
				'src/core/SkConfig8888.cpp',
				'src/core/SkConvolver.cpp',
				'src/core/SkCpu.cpp',
				'src/core/SkCubicClipper.cpp',
				'src/core/SkData.cpp',
				'src/core/SkDataTable.cpp',
				'src/core/SkDebug.cpp',
				'src/core/SkDeque.cpp',
				'src/core/SkDevice.cpp',
				'src/core/SkDeviceLooper.cpp',
				'src/core/SkDeviceProfile.cpp',
				'src/core/SkDistanceFieldGen.cpp',
				'src/core/SkDither.cpp',
				'src/core/SkDocument.cpp',
				'src/core/SkDraw.cpp',
				'src/core/SkDrawable.cpp',
				'src/core/SkDrawLooper.cpp',
				'src/core/SkEdge.cpp',
				'src/core/SkEdgeBuilder.cpp',
				'src/core/SkEdgeClipper.cpp',
				'src/core/SkFilterProc.cpp',
				'src/core/SkFixedAlloc.cpp',
				'src/core/SkFlattenable.cpp',
				'src/core/SkFlattenableSerialization.cpp',
				'src/core/SkFont.cpp',
				'src/core/SkFontDescriptor.cpp',
				'src/core/SkFontLCDConfig.cpp',
				'src/core/SkFontMgr.cpp',
				'src/core/SkFontStream.cpp',
				'src/core/SkFontStyle.cpp',
				'src/core/SkForceCPlusPlusLinking.cpp',
				'src/core/SkGeometry.cpp',
				'src/core/SkGlobalInitialization_core.cpp',
				'src/core/SkGlyphCache.cpp',
				'src/core/SkGraphics.cpp',
				'src/core/SkHalf.cpp',
				'src/core/SkImageCacherator.cpp',
				'src/core/SkImageFilter.cpp',
				'src/core/SkImageFilterCache.cpp',
				'src/core/SkImageGenerator.cpp',
				'src/core/SkImageInfo.cpp',
				'src/core/SkLatticeIter.cpp',
				'src/core/SkLightingShader.cpp',
				'src/core/SkLights.cpp',
				'src/core/SkLinearBitmapPipeline.cpp',
				'src/core/SkLineClipper.cpp',
				'src/core/SkLiteDL.cpp',
				'src/core/SkLiteRecorder.h',
				'src/core/SkLocalMatrixImageFilter.cpp',
				'src/core/SkLocalMatrixShader.cpp',
				'src/core/SkMallocPixelRef.cpp',
				'src/core/SkMask.cpp',
				'src/core/SkMaskCache.cpp',
				'src/core/SkMaskFilter.cpp',
				'src/core/SkMaskGamma.cpp',
				'src/core/SkMath.cpp',
				'src/core/SkMatrix.cpp',
				'src/core/SkMatrix44.cpp',
				'src/core/SkMatrixImageFilter.cpp',
				'src/core/SkMD5.cpp',
				'src/core/SkMetaData.cpp',
				'src/core/SkMiniRecorder.cpp',
				'src/core/SkMipMap.cpp',
				'src/core/SkModeColorFilter.cpp',
				'src/core/SkMultiPictureDraw.cpp',
				'src/core/SkNormalBevelSource.cpp',
				'src/core/SkNormalFlatSource.cpp',
				'src/core/SkNormalMapSource.cpp',
				'src/core/SkNormalSource.cpp',
				'src/core/SkOpts.cpp',
				'src/core/SkOverdrawCanvas.cpp',
				'src/core/SkPaint.cpp',
				'src/core/SkPaintPriv.cpp',
				'src/core/SkPath.cpp',
				'src/core/SkPathEffect.cpp',
				'src/core/SkPathMeasure.cpp',
				'src/core/SkPathRef.cpp',
				'src/core/SkPicture.cpp',
				'src/core/SkPictureAnalyzer.cpp',
				'src/core/SkPictureContentInfo.cpp',
				'src/core/SkPictureData.cpp',
				'src/core/SkPictureFlat.cpp',
				'src/core/SkPictureImageGenerator.cpp',
				'src/core/SkPicturePlayback.cpp',
				'src/core/SkPictureRecord.cpp',
				'src/core/SkPictureRecorder.cpp',
				'src/core/SkPictureShader.cpp',
				'src/core/SkPixelRef.cpp',
				'src/core/SkPixmap.cpp',
				'src/core/SkPoint.cpp',
				'src/core/SkPoint3.cpp',
				'src/core/SkPtrRecorder.cpp',
				'src/core/SkQuadClipper.cpp',
				'src/core/SkRadialShadowMapShader.cpp',
				'src/core/SkRasterClip.cpp',
				'src/core/SkRasterizer.cpp',
				'src/core/SkRasterPipeline.cpp',
				'src/core/SkRasterPipelineBlitter.cpp',
				'src/core/SkReadBuffer.cpp',
				'src/core/SkRecord.cpp',
				'src/core/SkRecordDraw.cpp',
				'src/core/SkRecordedDrawable.cpp',
				'src/core/SkRecorder.cpp',
				'src/core/SkRecordOpts.cpp',
				'src/core/SkRecords.cpp',
				'src/core/SkRect.cpp',
				'src/core/SkRefDict.cpp',
				'src/core/SkRegion.cpp',
				'src/core/SkRegion_path.cpp',
				'src/core/SkResourceCache.cpp',
				'src/core/SkRRect.cpp',
				'src/core/SkRTree.cpp',
				'src/core/SkRWBuffer.cpp',
				'src/core/SkScalar.cpp',
				'src/core/SkScalerContext.cpp',
				'src/core/SkScan.cpp',
				'src/core/SkScan_AAAPath.cpp',
				'src/core/SkScan_Antihair.cpp',
				'src/core/SkScan_AntiPath.cpp',
				'src/core/SkScan_Hairline.cpp',
				'src/core/SkScan_Path.cpp',
				'src/core/SkSemaphore.cpp',
				'src/core/SkShader.cpp',
				'src/core/SkShadowShader.cpp',
				'src/core/SkSharedMutex.cpp',
				'src/core/SkSpanProcs.cpp',
				'src/core/SkSpecialImage.cpp',
				'src/core/SkSpecialSurface.cpp',
				'src/core/SkSpinlock.cpp',
				'src/core/SkSpriteBlitter_ARGB32.cpp',
				'src/core/SkSpriteBlitter_RGB16.cpp',
				'src/core/SkSpriteBlitter4f.cpp',
				'src/core/SkSRGB.cpp',
				'src/core/SkStream.cpp',
				'src/core/SkString.cpp',
				'src/core/SkStringUtils.cpp',
				'src/core/SkStroke.cpp',
				'src/core/SkStrokeRec.cpp',
				'src/core/SkStrokerPriv.cpp',
				'src/core/SkSwizzle.cpp',
				'src/core/SkTaskGroup.cpp',
				'src/core/SkTextBlob.cpp',
				'src/core/SkThreadID.cpp',
				'src/core/SkTime.cpp',
				'src/core/SkTLS.cpp',
				'src/core/SkTSearch.cpp',
				'src/core/SkTypeface.cpp',
				'src/core/SkTypefaceCache.cpp',
				'src/core/SkUnPreMultiply.cpp',
				'src/core/SkUtils.cpp',
				'src/core/SkUtilsArm.cpp',
				'src/core/SkValidatingReadBuffer.cpp',
				'src/core/SkVarAlloc.cpp',
				'src/core/SkVertState.cpp',
				'src/core/SkWriteBuffer.cpp',
				'src/core/SkWriter32.cpp',
				'src/core/SkXfermode.cpp',
				'src/core/SkXfermode4f.cpp',
				'src/core/SkXfermodeF16.cpp',
				'src/core/SkXfermodeInterpretation.cpp',
				'src/core/SkYUVPlanesCache.cpp',
				
				'src/effects/SkArithmeticModePriv.h',
				'src/effects/SkBlurMask.h',
				'src/effects/SkEmbossMask.h',
				'src/effects/SkEmbossMask_Table.h',
				'src/effects/SkOverdrawColorFilter.h',
				'src/effects/Sk1DPathEffect.cpp',
				'src/effects/Sk2DPathEffect.cpp',
				'src/effects/SkAlphaThresholdFilter.cpp',
				'src/effects/SkArcToPathEffect.cpp',
				'src/effects/SkArithmeticMode.cpp',
				'src/effects/SkBlurDrawLooper.cpp',
				'src/effects/SkBlurMask.cpp',
				'src/effects/SkBlurMaskFilter.cpp',
				'src/effects/SkColorCubeFilter.cpp',
				'src/effects/SkColorFilterImageFilter.cpp',
				'src/effects/SkColorMatrix.cpp',
				'src/effects/SkColorMatrixFilter.cpp',
				'src/effects/SkComposeImageFilter.cpp',
				'src/effects/SkCornerPathEffect.cpp',
				'src/effects/SkDashPathEffect.cpp',
				'src/effects/SkDiscretePathEffect.cpp',
				'src/effects/SkDisplacementMapEffect.cpp',
				'src/effects/SkDropShadowImageFilter.cpp',
				'src/effects/SkEmbossMask.cpp',
				'src/effects/SkEmbossMaskFilter.cpp',
				'src/effects/SkGammaColorFilter.cpp',
				'src/effects/SkGaussianEdgeShader.cpp',
				'src/effects/SkImageSource.cpp',
				'src/effects/SkLayerDrawLooper.cpp',
				'src/effects/SkLayerRasterizer.cpp',
				'src/effects/SkLightingImageFilter.cpp',
				'src/effects/SkLumaColorFilter.cpp',
				'src/effects/SkMagnifierImageFilter.cpp',
				'src/effects/SkMatrixConvolutionImageFilter.cpp',
				'src/effects/SkMergeImageFilter.cpp',
				'src/effects/SkMorphologyImageFilter.cpp',
				'src/effects/SkOffsetImageFilter.cpp',
				'src/effects/SkOverdrawColorFilter.cpp',
				'src/effects/SkPackBits.cpp',
				'src/effects/SkPaintFlagsDrawFilter.cpp',
				'src/effects/SkPaintImageFilter.cpp',
				'src/effects/SkPerlinNoiseShader.cpp',
				'src/effects/SkPictureImageFilter.cpp',
				'src/effects/SkRRectsGaussianEdgeMaskFilter.cpp',
				'src/effects/SkShadowMaskFilter.cpp',
				'src/effects/SkTableColorFilter.cpp',
				'src/effects/SkTableMaskFilter.cpp',
				'src/effects/SkTileImageFilter.cpp',
				'src/effects/SkXfermodeImageFilter.cpp',
				
				'src/effects/gradients/Sk4fGradientBase.h',
				'src/effects/gradients/Sk4fGradientPriv.h',
				'src/effects/gradients/Sk4fLinearGradient.h',
				'src/effects/gradients/SkClampRange.h',
				'src/effects/gradients/SkGradientBitmapCache.h',
				'src/effects/gradients/SkGradientShaderPriv.h',
				'src/effects/gradients/SkLinearGradient.h',
				'src/effects/gradients/SkRadialGradient.h',
				'src/effects/gradients/SkSweepGradient.h',
				'src/effects/gradients/SkTwoPointConicalGradient.h',
				'src/effects/gradients/Sk4fGradientBase.cpp',
				'src/effects/gradients/Sk4fLinearGradient.cpp',
				'src/effects/gradients/SkClampRange.cpp',
				'src/effects/gradients/SkGradientBitmapCache.cpp',
				'src/effects/gradients/SkGradientShader.cpp',
				'src/effects/gradients/SkLinearGradient.cpp',
				'src/effects/gradients/SkRadialGradient.cpp',
				'src/effects/gradients/SkSweepGradient.cpp',
				'src/effects/gradients/SkTwoPointConicalGradient.cpp',
				
				'src/fonts/SkGScalerContext.h',
				'src/fonts/SkRandomScalerContext.h',
				'src/fonts/SkTestScalerContext.h',
				'src/fonts/SkFontMgr_indirect.cpp',
				'src/fonts/SkGScalerContext.cpp',
				'src/fonts/SkRandomScalerContext.cpp',
				'src/fonts/SkRemotableFontMgr.cpp',
				'src/fonts/SkTestScalerContext.cpp',
            
				
				'src/image/SkImage_Base.h',
				'src/image/SkImageShader.h',
				'src/image/SkImageShaderContext.h',
				'src/image/SkReadPixelsRec.h',
				'src/image/SkSurface_Base.h',
				'src/image/SkImage.cpp',
				'src/image/SkImage_Generator.cpp',
				'src/image/SkImage_Raster.cpp',
				'src/image/SkImageShader.cpp',
				'src/image/SkSurface.cpp',
				'src/image/SkSurface_Raster.cpp',
				
				'src/images/transform_scanline.h',
				'src/images/SkImageEncoderPriv.h',
				'src/images/SkJPEGWriteUtility.h',
				'src/images/SkImageEncoder.cpp',
				'src/images/SkJPEGImageEncoder.cpp',
				'src/images/SkJPEGWriteUtility.cpp',
				'src/images/SkKTXImageEncoder.cpp',
				'src/images/SkPNGImageEncoder.cpp',
				'src/images/SkWEBPImageEncoder.cpp',
				
				'src/lazy/SkDiscardableMemoryPool.h',
				'src/lazy/SkDiscardableMemoryPool.cpp',
				
				'src/opts/Sk4px_NEON.h',
				'src/opts/Sk4px_none.h',
				'src/opts/Sk4px_SSE2.h',
				'src/opts/SkBitmapFilter_opts.h',
				'src/opts/SkBitmapProcState_filter_neon.h',
				'src/opts/SkBitmapProcState_matrix_neon.h',
				'src/opts/SkBitmapProcState_opts_SSE2.h',
				'src/opts/SkBitmapProcState_opts_SSSE3.h',
				'src/opts/SkBlend_opts.h',
				'src/opts/SkBlitMask_opts.h',
				'src/opts/SkBlitMask_opts_arm_neon.h',
				'src/opts/SkBlitRow_opts.h',
				'src/opts/SkBlitRow_opts_arm_neon.h',
				'src/opts/SkBlitRow_opts_SSE2.h',
				'src/opts/SkBlurImageFilter_opts.h',
				'src/opts/SkChecksum_opts.h',
				'src/opts/SkColor_opts_neon.h',
				'src/opts/SkColor_opts_SSE2.h',
				'src/opts/SkColorCubeFilter_opts.h',
				'src/opts/SkMorphologyImageFilter_opts.h',
				'src/opts/SkNx_neon.h',
				'src/opts/SkNx_sse.h',
				'src/opts/SkRasterPipeline_opts.h',
				'src/opts/SkSwizzler_opts.h',
				'src/opts/SkTextureCompressor_opts.h',
				'src/opts/SkXfermode_opts.h',
				
				'src/pathops/SkAddIntersections.h',
				'src/pathops/SkIntersectionHelper.h',
				'src/pathops/SkIntersections.h',
				'src/pathops/SkLineParameters.h',
				'src/pathops/SkOpAngle.h',
				'src/pathops/SkOpCoincidence.h',
				'src/pathops/SkOpContour.h',
				'src/pathops/SkOpEdgeBuilder.h',
				'src/pathops/SkOpSegment.h',
				'src/pathops/SkOpSpan.h',
				'src/pathops/SkPathOpsBounds.h',
				'src/pathops/SkPathOpsCommon.h',
				'src/pathops/SkPathOpsConic.h',
				'src/pathops/SkPathOpsCubic.h',
				'src/pathops/SkPathOpsCurve.h',
				'src/pathops/SkPathOpsDebug.h',
				'src/pathops/SkPathOpsLine.h',
				'src/pathops/SkPathOpsPoint.h',
				'src/pathops/SkPathOpsQuad.h',
				'src/pathops/SkPathOpsRect.h',
				'src/pathops/SkPathOpsTSect.h',
				'src/pathops/SkPathOpsTypes.h',
				'src/pathops/SkPathWriter.h',
				'src/pathops/SkReduceOrder.h',
				'src/pathops/SkAddIntersections.cpp',
				'src/pathops/SkDConicLineIntersection.cpp',
				'src/pathops/SkDCubicLineIntersection.cpp',
				'src/pathops/SkDCubicToQuads.cpp',
				'src/pathops/SkDLineIntersection.cpp',
				'src/pathops/SkDQuadLineIntersection.cpp',
				'src/pathops/SkIntersections.cpp',
				'src/pathops/SkOpAngle.cpp',
				'src/pathops/SkOpBuilder.cpp',
				'src/pathops/SkOpCoincidence.cpp',
				'src/pathops/SkOpContour.cpp',
				'src/pathops/SkOpCubicHull.cpp',
				'src/pathops/SkOpEdgeBuilder.cpp',
				'src/pathops/SkOpSegment.cpp',
				'src/pathops/SkOpSpan.cpp',
				'src/pathops/SkPathOpsCommon.cpp',
				'src/pathops/SkPathOpsConic.cpp',
				'src/pathops/SkPathOpsCubic.cpp',
				'src/pathops/SkPathOpsCurve.cpp',
				'src/pathops/SkPathOpsDebug.cpp',
				'src/pathops/SkPathOpsLine.cpp',
				'src/pathops/SkPathOpsOp.cpp',
				'src/pathops/SkPathOpsPoint.cpp',
				'src/pathops/SkPathOpsQuad.cpp',
				'src/pathops/SkPathOpsRect.cpp',
				'src/pathops/SkPathOpsSimplify.cpp',
				'src/pathops/SkPathOpsTightBounds.cpp',
				'src/pathops/SkPathOpsTSect.cpp',
				'src/pathops/SkPathOpsTypes.cpp',
				'src/pathops/SkPathOpsWinding.cpp',
				'src/pathops/SkPathWriter.cpp',
				'src/pathops/SkReduceOrder.cpp',
				
				'src/pdf/SkBitmapKey.h',
				'src/pdf/SkDeflate.h',
				'src/pdf/SkJpegInfo.h',
				'src/pdf/SkPDFBitmap.h',
				'src/pdf/SkPDFCanon.h',
				'src/pdf/SkPDFCanvas.h',
				'src/pdf/SkPDFConvertType1FontStream.h',
				'src/pdf/SkPDFDevice.h',
				'src/pdf/SkPDFFont.h',
				'src/pdf/SkPDFFormXObject.h',
				'src/pdf/SkPDFGraphicState.h',
				'src/pdf/SkPDFMakeCIDGlyphWidthsArray.h',
				'src/pdf/SkPDFMakeToUnicodeCmap.h',
				'src/pdf/SkPDFMetadata.h',
				'src/pdf/SkPDFResourceDict.h',
				'src/pdf/SkPDFShader.h',
				'src/pdf/SkPDFTypes.h',
				'src/pdf/SkPDFUtils.h',
				'src/pdf/SkScopeExit.h',
				'src/pdf/SkDeflate.cpp',
				'src/pdf/SkJpegInfo.cpp',
				'src/pdf/SkPDFBitmap.cpp',
				'src/pdf/SkPDFCanon.cpp',
				'src/pdf/SkPDFCanvas.cpp',
				'src/pdf/SkPDFConvertType1FontStream.cpp',
				'src/pdf/SkPDFDevice.cpp',
				'src/pdf/SkPDFDocument.cpp',
				'src/pdf/SkPDFFont.cpp',
				'src/pdf/SkPDFFormXObject.cpp',
				'src/pdf/SkPDFGraphicState.cpp',
				'src/pdf/SkPDFMakeCIDGlyphWidthsArray.cpp',
				'src/pdf/SkPDFMakeToUnicodeCmap.cpp',
				'src/pdf/SkPDFMetadata.cpp',
				'src/pdf/SkPDFResourceDict.cpp',
				'src/pdf/SkPDFShader.cpp',
				'src/pdf/SkPDFTypes.cpp',
				'src/pdf/SkPDFUtils.cpp',
				
				'src/pipe/SkPipeCanvas.h',
				'src/pipe/SkPipeFormat.h',
				'src/pipe/SkRefSet.h',
				'src/pipe/SkPipeCanvas.cpp',
				'src/pipe/SkPipeReader.cpp',
				
				'src/ports/SkFontConfigInterface_direct.h',
				'src/ports/SkFontConfigTypeface.h',
				'src/ports/SkFontHost_FreeType_common.h',
				'src/ports/SkFontMgr_android_parser.h',
				'src/ports/SkImageGeneratorCG.h',
				'src/ports/SkImageGeneratorWIC.h',
				'src/ports/SkOSLibrary.h',
				'src/ports/SkScalerContext_win_dw.h',
				'src/ports/SkTypeface_win_dw.h',
				'src/ports/SkDebug_android.cpp',
				'src/ports/SkDebug_stdio.cpp',
				'src/ports/SkDebug_win.cpp',
				'src/ports/SkDiscardableMemory_none.cpp',
				'src/ports/SkFontConfigInterface.cpp',
				'src/ports/SkFontConfigInterface_direct.cpp',
				'src/ports/SkFontConfigInterface_direct_factory.cpp',
				'src/ports/SkFontHost_FreeType.cpp',
				'src/ports/SkFontHost_FreeType_common.cpp',
				'src/ports/SkFontHost_mac.cpp',
				'src/ports/SkFontHost_win.cpp',
				'src/ports/SkFontMgr_android.cpp',
				'src/ports/SkFontMgr_android_factory.cpp',
				'src/ports/SkFontMgr_android_parser.cpp',
				'src/ports/SkFontMgr_custom.cpp',
				'src/ports/SkFontMgr_custom_directory_factory.cpp',
				'src/ports/SkFontMgr_custom_embedded_factory.cpp',
				'src/ports/SkFontMgr_custom_empty_factory.cpp',
				'src/ports/SkFontMgr_fontconfig.cpp',
				'src/ports/SkFontMgr_fontconfig_factory.cpp',
				'src/ports/SkFontMgr_FontConfigInterface.cpp',
				'src/ports/SkFontMgr_FontConfigInterface_factory.cpp',
				'src/ports/SkFontMgr_win_dw.cpp',
				'src/ports/SkFontMgr_win_dw_factory.cpp',
				'src/ports/SkFontMgr_win_gdi_factory.cpp',
				'src/ports/SkGlobalInitialization_default.cpp',
				'src/ports/SkImageEncoder_CG.cpp',
				'src/ports/SkImageEncoder_none.cpp',
				'src/ports/SkImageEncoder_WIC.cpp',
				'src/ports/SkImageGenerator_none.cpp',
				'src/ports/SkImageGenerator_skia.cpp',
				'src/ports/SkImageGeneratorCG.cpp',
				'src/ports/SkImageGeneratorWIC.cpp',
				'src/ports/SkMemory_malloc.cpp',
				'src/ports/SkMemory_mozalloc.cpp',
				'src/ports/SkOSFile_posix.cpp',
				'src/ports/SkOSFile_stdio.cpp',
				'src/ports/SkOSFile_win.cpp',
				'src/ports/SkOSLibrary_posix.cpp',
				'src/ports/SkOSLibrary_win.cpp',
				'src/ports/SkRemotableFontMgr_win_dw.cpp',
				'src/ports/SkScalerContext_win_dw.cpp',
				'src/ports/SkTLS_none.cpp',
				'src/ports/SkTLS_pthread.cpp',
				'src/ports/SkTLS_win.cpp',
				'src/ports/SkTypeface_win_dw.cpp',
				
				'src/sfnt/SkIBMFamilyClass.h',
				'src/sfnt/SkOTTable_EBDT.h',
				'src/sfnt/SkOTTable_EBLC.h',
				'src/sfnt/SkOTTable_EBSC.h',
				'src/sfnt/SKOTTable_gasp.h',
				'src/sfnt/SKOTTable_glyf.h',
				'src/sfnt/SkOTTable_head.h',
				'src/sfnt/SkOTTable_hhea.h',
				'src/sfnt/SkOTTable_loca.h',
				'src/sfnt/SkOTTable_maxp.h',
				'src/sfnt/SkOTTable_maxp_CFF.h',
				'src/sfnt/SkOTTable_maxp_TT.h',
				'src/sfnt/SkOTTable_name.h',
				'src/sfnt/SkOTTable_OS_2.h',
				'src/sfnt/SkOTTable_OS_2_V0.h',
				'src/sfnt/SkOTTable_OS_2_V1.h',
				'src/sfnt/SkOTTable_OS_2_V2.h',
				'src/sfnt/SkOTTable_OS_2_V3.h',
				'src/sfnt/SkOTTable_OS_2_V4.h',
				'src/sfnt/SkOTTable_OS_2_VA.h',
				'src/sfnt/SkOTTable_post.h',
				'src/sfnt/SkOTTableTypes.h',
				'src/sfnt/SkOTUtils.h',
				'src/sfnt/SkPanose.h',
				'src/sfnt/SkSFNTHeader.h',
				'src/sfnt/SkTTCFHeader.h',
				'src/sfnt/SkOTTable_name.cpp',
				'src/sfnt/SkOTUtils.cpp',
								
				'src/svg/SkSVGDevice.h',
				'src/svg/SkSVGCanvas.cpp',
				'src/svg/SkSVGDevice.cpp',
				
				'src/utils/SkBase64.h',
				'src/utils/SkBitmapSourceDeserializer.h',
				'src/utils/SkBitSet.h',
				'src/utils/SkCanvasStack.h',
				'src/utils/SkCurveMeasure.h',
				'src/utils/SkDashPathPriv.h',
				'src/utils/SkDeferredCanvas.h',
				'src/utils/SkFloatUtils.h',
				'src/utils/SkMatrix22.h',
				'src/utils/SkMultiPictureDocument.h',
				'src/utils/SkMultiPictureDocumentPriv.h',
				'src/utils/SkMultiPictureDocumentReader.h',
				'src/utils/SkOSPath.h',
				'src/utils/SkPatchGrid.h',
				'src/utils/SkPatchUtils.h',
				'src/utils/SkRGBAToYUV.h',
				'src/utils/SkShadowPaintFilterCanvas.h',
				'src/utils/SkTextureCompressor.h',
				'src/utils/SkTextureCompressor_ASTC.h',
				'src/utils/SkTextureCompressor_Blitter.h',
				'src/utils/SkTextureCompressor_LATC.h',
				'src/utils/SkTextureCompressor_R11EAC.h',
				'src/utils/SkTextureCompressor_Utils.h',
				'src/utils/SkThreadUtils.h',
				'src/utils/SkThreadUtils_pthread.h',
				'src/utils/SkThreadUtils_win.h',
				'src/utils/SkBase64.cpp',
				'src/utils/SkBitmapSourceDeserializer.cpp',
				'src/utils/SkBoundaryPatch.cpp',
				'src/utils/SkCamera.cpp',
				'src/utils/SkCanvasStack.cpp',
				'src/utils/SkCanvasStateUtils.cpp',
				'src/utils/SkCurveMeasure.cpp',
				'src/utils/SkDashPath.cpp',
				'src/utils/SkDeferredCanvas.cpp',
				'src/utils/SkDumpCanvas.cpp',
				'src/utils/SkEventTracer.cpp',
				'src/utils/SkFrontBufferedStream.cpp',
				'src/utils/SkInterpolator.cpp',
				'src/utils/SkLayer.cpp',
				'src/utils/SkLua.cpp',
				'src/utils/SkLuaCanvas.cpp',
				'src/utils/SkMatrix22.cpp',
				'src/utils/SkMeshUtils.cpp',
				'src/utils/SkMultiPictureDocument.cpp',
				'src/utils/SkMultiPictureDocumentReader.cpp',
				'src/utils/SkNullCanvas.cpp',
				'src/utils/SkNWayCanvas.cpp',
				'src/utils/SkOSPath.cpp',
				'src/utils/SkPaintFilterCanvas.cpp',
				'src/utils/SkParse.cpp',
				'src/utils/SkParseColor.cpp',
				'src/utils/SkParsePath.cpp',
				'src/utils/SkPatchGrid.cpp',
				'src/utils/SkPatchUtils.cpp',
				'src/utils/SkRGBAToYUV.cpp',
				'src/utils/SkShadowPaintFilterCanvas.cpp',
				'src/utils/SkTextBox.cpp',
				'src/utils/SkTextureCompressor.cpp',
				'src/utils/SkTextureCompressor_ASTC.cpp',
				'src/utils/SkTextureCompressor_LATC.cpp',
				'src/utils/SkTextureCompressor_R11EAC.cpp',
				'src/utils/SkThreadUtils_pthread.cpp',
				'src/utils/SkThreadUtils_win.cpp',
				'src/utils/SkWhitelistTypefaces.cpp',
				
				'src/utils/mac/SkCreateCGImageRef.cpp',
				'src/utils/mac/SkStream_mac.cpp',
				
				'src/utils/win/SkAutoCoInitialize.h',
				'src/utils/win/SkDWrite.h',
				'src/utils/win/SkDWriteFontFileStream.h',
				'src/utils/win/SkDWriteGeometrySink.h',
				'src/utils/win/SkHRESULT.h',
				'src/utils/win/SkIStream.h',
				'src/utils/win/SkTScopedComPtr.h',
				'src/utils/win/SkWGL.h',
				'src/utils/win/SkAutoCoInitialize.cpp',
				'src/utils/win/SkDWrite.cpp',
				'src/utils/win/SkDWriteFontFileStream.cpp',
				'src/utils/win/SkDWriteGeometrySink.cpp',
				'src/utils/win/SkHRESULT.cpp',
				'src/utils/win/SkIStream.cpp',
				'src/utils/win/SkWGL_win.cpp',
				
				# TODO: views subdirectory
				
				'src/xml/SkDOM.cpp',
				'src/xml/SkXMLParser.cpp',
				'src/xml/SkXMLWriter.cpp',
				
				'src/xps/SkXPSDevice.h',
				'src/xps/SkDocument_XPS.cpp',
				'src/xps/SkDocument_XPS_None.cpp',
				'src/xps/SkXPSDevice.cpp',
			],
			
			'defines':
			[
				'<@(skia_defines)',
			],
			
			'all_dependent_settings':
			{
				'defines':
				[
					# Disable Skia debugging
					'SK_RELEASE',
				
					# We use deprecated Skia features
					'SK_SUPPORT_LEGACY_CANVAS_IS_REFCNT',
					'SK_SUPPORT_LEGACY_GETTOPDEVICE',
					'SK_SUPPORT_LEGACY_ACCESSBITMAP',
					'SK_SUPPORT_LEGACY_CLIP_REGIONOPS',
					'SK_SUPPORT_LEGACY_GETDEVICE',
                    
                    # Disable GPU support
                    'SK_SUPPORT_GPU=0',
				],
			},
			
			'sources!':
			[
				# Not relevant to the plaforms we support
				'src/ports/SkMemory_mozalloc.cpp',
				
				# Don't build the LUA bindings
				'src/utils/SkLua.cpp',
				'src/utils/SkLuaCanvas.cpp',
			],
			
			'sources/':
			[
				# Disable all image codecs (we don't use Skia for image encode/decode)
				['exclude', 'KTX'],
				['exclude', '(W|w)(E|e)(B|b)(P|p)'],
				['exclude', '^src/codec/.*Codec.*\\.cpp$'],
				['include', '^src/codec/SkCodec(ImageGenerator)?\\.cpp$'],
				
				# Disable animation scripting
				['exclude', 'src/animator/SkScript'],
				
				# SkSnapshot doesn't build
				['exclude', 'src/animator/SkSnapshot'],
				
				# Don't build XPS 
				['exclude', '/xps/'],
			],
			
			'conditions':
			[
				[
					'OS in ("emscripten", "android")',
					{
						'dependencies':
						[
							'../libfreetype/libfreetype.gyp:libfreetype',
						],
					}
				],
			],

			'target_conditions':
			[
				[
					'toolset_os == "win"',
					{
						'include_dirs':
						[
							'src/utils/win',
						],
						
						'sources!':
						[
							# Disable generic files where Win32-specific support is available
							'src/ports/SkImageEncoder_none.cpp',
							'src/ports/SkImageGenerator_none.cpp',
							'src/ports/SkTLS_none.cpp',
							
							# Use DirectWrite instead of GDI
							'src/ports/SkFontMgr_win_gdi_factory.cpp',
						],
						
						'sources/':
						[
							# Disable POSIX features
							['exclude', '(pthread|posix)\\.cpp$'],
							
							# Don't build FreeType or FontConfig code
							['exclude', 'freetype'],
							['exclude', 'FreeType'],
							['exclude', 'fontconfig'],
							['exclude', 'FontConfig'],
							['exclude', 'SkFontMgr_custom'],
						],
						
						'defines':
						[
							# Ensure we get the complete Windows Imaging Components headers
							'_WIN7_PLATFORM_UPDATE',

							# Disable Analytical AnitAliasing on Windows
							# TODO: Recheck rounded-rectangle appearence after next Skia update.
							'SK_NO_ANALYTIC_AA',
						],
					},
				],
				[
					'toolset_os != "win"',
					{
						'sources/':
						[
							# Disable windows-specific code
							['exclude', '_win(_dw)?.*\\.cpp$'],
							['exclude', '^src/.*/win/.*$'],
                            ['exclude', 'WIC\\.cpp$'],
						],
					},
				],
                [
                    'toolset_os in ("mac", "ios")',
                    {
                        'sources!':
                        [
                            # Disable generic files where macOS/iOS support is available
                            'src/ports/SkImageEncoder_none.cpp',
                            'src/ports/SkImageGenerator_none.cpp',
                            'src/ports/SkTLS_none.cpp',
                            
                            # Disable un-needed features
                            # (or at least the ones that don't compile!)
                        ],
                        
                        'sources/':
                        [
                            # Don't build FreeType or FontConfig code
                            ['exclude', 'freetype'],
                            ['exclude', 'FreeType'],
                            ['exclude', 'fontconfig'],
                            ['exclude', 'FontConfig'],
                            ['exclude', 'SkFontMgr_custom'],
                        ],
                    },
                ],
				[
					'toolset_os not in ("mac","ios")',
					{
						'sources/':
						[
							# Disable anything CoreGraphics related
							['exclude', 'CG\\.'],
							['exclude', '_mac\\.cpp$'],
						],
					},
				],
                [
                    'toolset_os != "emscripten"',
                    {
                    },
                ],
				[
					'toolset_os in ("linux", "emscripten")',
					{
						# Work around unreliable platform detection in the Skia headers
						'defines':
						[
							'SK_BUILD_FOR_UNIX',
						],
					},
				],
				[
					'toolset_os != "android"',
					{
						'sources/':
						[
							# Exclude all Android-specific sources
							['exclude', '_android'],
						],
					},
				],
				[
					'toolset_os != "android"',
					{
						'sources!':
						[
							'src/ports/SkFontHost_fontconfig.cpp',
                            'src/fonts/SkFontMgr_fontconfig.cpp',
						],
					},
				],
                [
                    'toolset_os == "emscripten"',
                    {
                        'defines':
                        [
                            'SK_FONT_FILE_PREFIX="/boot/standalone/"',
                        ],

                        'sources!':
						[
                            'src/ports/SkDebug_stdio.cpp',
							'src/ports/SkFontHost_none.cpp',
                            'src/ports/SkImageEncoder_none.cpp',
                            'src/ports/SkImageGenerator_none.cpp',
							'src/ports/SkOSFile_none.cpp',
							'src/ports/SkTLS_none.cpp',
						],
                        
                        'sources/':
                        [
                            # Don't build FreeType or FontConfig code
                            ['exclude', 'fontconfig'],
                            ['exclude', 'FontConfig'],
                            ['exclude', 'SkFontMgr_custom_(embedded|empty)'],	
						],
                    },
                ],
				[
					'toolset_os == "mac"',
					{
                        'sources/':
                        [
                        ],
					},
				],
				[
					'toolset_os == "ios"',
					{
						'sources!':
						[
							'src/ports/SkOSFile_none.cpp',
							'src/ports/SkTLS_none.cpp',
						],
					},
				],
                [
                    'toolset_os == "linux"',
                    {
                        'sources!':
						[
							'src/ports/SkFontHost_none.cpp',
                            'src/ports/SkImageEncoder_none.cpp',
                            'src/ports/SkImageGenerator_none.cpp',
							'src/ports/SkOSFile_none.cpp',
							'src/ports/SkTLS_none.cpp',
						],
                        
                        'sources/':
                        [
                            ['exclude', 'SkFontMgr_custom'],
                        ],
                    }
                ],
				[
					'toolset_os == "android"',
					{
						'defines':
						[
                            'SK_BUILD_FOR_ANDROID',
                            'SK_BUILD_FOR_ANDROID_NDK',
						],
                        
                        # Need to include the cpufeatures module from the Android NDK
						'variables':
						{
							# Default value because only Android defines the <(android_ndk_path) variable.
							'android_ndk_path%': '',
						},
                        'include_dirs':
                        [
                            '<(android_ndk_path)/sources/android/cpufeatures',
                        ],
                        'sources':
                        [
                            'src/ports/android-cpu-features.c',
                        ],

						'sources!':
						[
                            'src/ports/SkDebug_stdio.cpp',
							'src/ports/SkFontHost_none.cpp',
                            'src/ports/SkImageEncoder_none.cpp',
                            'src/ports/SkImageGenerator_none.cpp',
							'src/ports/SkOSFile_none.cpp',
							'src/ports/SkTLS_none.cpp',
						],
                        
                        'sources/':
                        [
                            # Don't build FreeType or FontConfig code
                            ['exclude', 'fontconfig'],
                            ['exclude', 'FontConfig'],
                            ['exclude', 'SkFontMgr_custom'],
                        ],
                    },                      
                ],
			],

			'link_settings':
			{
				'target_conditions':
				[
					[
						'toolset_os == "win"',
						{
							'libraries':
							[
								#'-lOpenGL32.lib'
							],
						},
					],
					[
						'toolset_os == "linux"',
						{
							'libraries':
							[
								#'-lGL',
								'-lfreetype',
								'-lfontconfig',
							],
						},
					],
					[
						'toolset_os == "android"',
						{
							'libraries':
							[
								#'-lEGL',
								#'-lGLESv2',
							],
						},
					],
				],
			},
			
			'direct_dependent_settings':
			{
				'include_dirs':
				[
					'include/config',
					'include/core',
					'include/device',
					'include/effects',
					'include/images',
					'include/pathops',
					'include/pdf',
					'include/pipe',
					'include/ports',
					'include/svg',
					'include/text',
					'include/utils',
                    
                    # Needed for some legacy methods
                    'src/core',
					# Needed for directwrite text rendering on Windows
					'src/ports',
					'include/private',
					'src/utils/win',
				],
			},
		},
	],
}
