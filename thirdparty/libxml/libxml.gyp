{
	'includes':
	[
		'../../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'libxml',
			
			'conditions':
			[
				[
					'use_system_libxml == 0',
					{
						'type': 'static_library',
						
						'variables':
						{
							'library_for_module': 1,
							'silence_warnings': 1,
						},
						
						'dependencies':
						[
							'../libz/libz.gyp:libz',
						],
						
						'include_dirs':
						[
							'include',
							'src',
						],
						
						'defines':
						[
							'LIBXML_STATIC_FOR_DLL=1',
							'TRIO_HAVE_CONFIG_H=1',
						],
						
						'sources':
						[
							'include/libxml/c14n.h',
							'include/libxml/catalog.h',
							'include/libxml/chvalid.h',
							'include/libxml/debugXML.h',
							'include/libxml/dict.h',
							'include/libxml/DOCBparser.h',
							'include/libxml/encoding.h',
							'include/libxml/entities.h',
							'include/libxml/globals.h',
							'include/libxml/hash.h',
							'include/libxml/HTMLparser.h',
							'include/libxml/HTMLtree.h',
							'include/libxml/list.h',
							'include/libxml/nanoftp.h',
							'include/libxml/nanohttp.h',
							'include/libxml/parser.h',
							'include/libxml/parserInternals.h',
							'include/libxml/pattern.h',
							'include/libxml/relaxng.h',
							'include/libxml/SAX.h',
							'include/libxml/SAX2.h',
							'include/libxml/schemasInternals.h',
							'include/libxml/schematron.h',
							'include/libxml/threads.h',
							'include/libxml/tree.h',
							'include/libxml/uri.h',
							'include/libxml/valid.h',
							'include/libxml/xinclude.h',
							'include/libxml/xlink.h',
							'include/libxml/xmlautomata.h',
							'include/libxml/xmlerror.h',
							'include/libxml/xmlexports.h',
							'include/libxml/xmlIO.h',
							'include/libxml/xmlmemory.h',
							'include/libxml/xmlmodule.h',
							'include/libxml/xmlreader.h',
							'include/libxml/xmlregexp.h',
							'include/libxml/xmlsave.h',
							'include/libxml/xmlschemas.h',
							'include/libxml/xmlschemastypes.h',
							'include/libxml/xmlstring.h',
							'include/libxml/xmlunicode.h',
							'include/libxml/xmlversion.h',
							'include/libxml/xmlwriter.h',
							'include/libxml/xpath.h',
							'include/libxml/xpathInternals.h',
							'include/libxml/xpointer.h',
							
							'src/acconfig.h',
							'src/buf.h',
							'src/config.h',
							'src/elfgcchack.h',
							'src/enc.h',
							'src/libxml.h',
							'src/save.h',
							'src/timsort.h',
							'src/trio.h',
							'src/triodef.h',
							'src/trionan.h',
							'src/triop.h',
							'src/triostr.h',
							'src/win32config.h',
							'src/wsockcompat.h',
							
							'src/buf.c',
							'src/c14n.c',
							'src/catalog.c',
							'src/chvalid.c',
							'src/debugXML.c',
							'src/dict.c',
							'src/DOCBparser.c',
							'src/encoding.c',
							'src/entities.c',
							'src/error.c',
							'src/globals.c',
							'src/hash.c',
							'src/HTMLparser.c',
							'src/HTMLtree.c',
							'src/legacy.c',
							'src/list.c',
							'src/nanoftp.c',
							'src/nanohttp.c',
							'src/parser.c',
							'src/parserInternals.c',
							'src/pattern.c',
							'src/relaxng.c',
							'src/SAX.c',
							'src/SAX2.c',
							'src/schematron.c',
							'src/threads.c',
							'src/tree.c',
							'src/trio.c',
							'src/trionan.c',
							'src/triostr.c',
							'src/uri.c',
							'src/valid.c',
							'src/xinclude.c',
							'src/xlink.c',
							'src/xmlcatalog.c',
							'src/xmlIO.c',
							'src/xmllint.c',
							'src/xmlmemory.c',
							'src/xmlmodule.c',
							'src/xmlreader.c',
							'src/xmlregexp.c',
							'src/xmlsave.c',
							'src/xmlschemas.c',
							'src/xmlschemastypes.c',
							'src/xmlstring.c',
							'src/xmlunicode.c',
							'src/xmlwriter.c',
							'src/xpath.c',
							'src/xpointer.c',
						],
						
						# xmllint is a separate tool
						'sources!':
						[
							'src/xmllint.c',
						],
						
						# The "trio*.c" and test files aren't needed
						'sources/':
						[
							['exclude', '^src/trio.*\\.c$'],
						],
						
						'direct_dependent_settings':
						{
							'include_dirs':
							[
								'include',
							],
							
							'defines':
							[
								'LIBXML_STATIC=1',
							],
						},
					},
					{
						'type': 'none',
						
						'link_settings':
						{
							'libraries':
							[
								'-lxml',
							],
						},
					},
				],
			],
		},
	],
}
