{
	'includes':
	[
		'../../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'libjpeg',

			'toolsets': ['host', 'target'],
			
			'conditions':
			[
				[
					'use_system_libjpeg == 0',
					{
						'type': 'static_library',
						
						'variables':
						{
							'silence_warnings': 1,
						},
						
						'include_dirs':
						[
							'include',
							'src',
						],
						
						'sources':
						[
							'include/jconfig.h',
							'include/jerror.h',
							'include/jmorecfg.h',
							'include/jpeglib.h',
							
							'src/jdct.h',
							'src/jinclude.h',
							'src/jmemsys.h',
							'src/jpegint.h',
							'src/jversion.h',
							
							'src/jaricom.c',
							'src/jcapimin.c',
							'src/jcapistd.c',
							'src/jcarith.c',
							'src/jccoefct.c',
							'src/jccolor.c',
							'src/jcdctmgr.c',
							'src/jchuff.c',
							'src/jcinit.c',
							'src/jcmainct.c',
							'src/jcmarker.c',
							'src/jcmaster.c',
							'src/jcomapi.c',
							'src/jcparam.c',
							'src/jcprepct.c',
							'src/jcsample.c',
							'src/jctrans.c',
							'src/jdapimin.c',
							'src/jdapistd.c',
							'src/jdarith.c',
							'src/jdatadst.c',
							'src/jdatasrc.c',
							'src/jdcoefct.c',
							'src/jdcolor.c',
							'src/jddctmgr.c',
							'src/jdhuff.c',
							'src/jdinput.c',
							'src/jdmainct.c',
							'src/jdmarker.c',
							'src/jdmaster.c',
							'src/jdmerge.c',
							'src/jdpostct.c',
							'src/jdsample.c',
							'src/jdtrans.c',
							'src/jerror.c',
							'src/jfdctflt.c',
							'src/jfdctfst.c',
							'src/jfdctint.c',
							'src/jidctflt.c',
							'src/jidctfst.c',
							'src/jidctint.c',
							'src/jmemmgr.c',
							'src/jmemnobs.c',
							'src/jquant1.c',
							'src/jquant2.c',
							'src/jutils.c',
						],
						
						'direct_dependent_settings':
						{
							'include_dirs':
							[
								'include',
							],
						},
					},
					{
						'type': 'none',
						
						'link_settings':
						{
							'libraries':
							[
								'-ljpeg',
							],
						},
					},
				],
			],
		},
	],
}
