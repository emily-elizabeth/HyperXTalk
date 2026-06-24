{
	'includes':
	[
		'../common.gypi',
		'engine-sources.gypi',
	],

	'targets':
	[
		{
			'target_name': 'kernel',
			'type': 'static_library',

			'dependencies':
			[
				'../libfoundation/libfoundation.gyp:libFoundation',
				'../libgraphics/libgraphics.gyp:libGraphics',
				'../libscript/libscript.gyp:libScript',
				'../libscript/libscript.gyp:stdscript',

				'../libbrowser/libbrowser.gyp:libbrowser',

				# libopenssl_stubs generates dlopen("revsecurity") wrappers for all
				# OpenSSL symbols.  On macOS, OpenSSL is statically linked via
				# libcustomssl.a / libcustomcrypto.a — there is no revsecurity.dylib,
				# so the stubs fail to load and SSL never initialises.  On macOS we
				# use openssl3_static_stubs.cpp instead (no-op initialise_weak_link_*
				# functions); the direct OpenSSL symbols resolve at link time.
				# On all other platforms keep the dynamic-loader stubs as before.
				# (dependency moved into conditions below)

				'../prebuilt/libopenssl.gyp:libopenssl_headers',

				'../thirdparty/libpcre/libpcre.gyp:libpcre',
				'../thirdparty/libjpeg/libjpeg.gyp:libjpeg',
				'../thirdparty/libgif/libgif.gyp:libgif',
				'../thirdparty/libpng/libpng.gyp:libpng',
				'../thirdparty/libvlc/libvlc.gyp:libvlc_headers',

				'../thirdparty/libz/libz.gyp:libz',

				'engine-common.gyp:encode_version',
				'engine-common.gyp:quicktime_stubs',
			],

			'include_dirs':
			[
				'include',
				'src',
				'../hxtlib',
				'../thirdparty/libpng/src',
				'../thirdparty/libjpeg/src',
			],

			'sources':
			[
				'<@(engine_common_source_files)',
				'<@(engine_desktop_source_files)',
				'<@(engine_module_source_files)',
				'<@(engine_java_source_files)',
				'../hxtlib/hxtlib.cpp',
			],

			'conditions':
			[
				[
					'OS == "linux"',
					{
						'include_dirs':
						[
							'../thirdparty/headers/linux/include/cairo',
							'<!@(pkg-config --cflags-only-I gtk+-3.0 2>/dev/null | sed "s/-I//g")',
							# gtk+-unix-print-3.0 adds /usr/include/gtk-3.0/unix-print,
							# required by lnxans.cpp's #include <gtk/gtkunixprint.h>
							'<!@(pkg-config --cflags-only-I gtk+-unix-print-3.0 2>/dev/null | sed "s/-I[^ ]*libpng[^ ]*//g" | sed "s/-I//g")',
							'<!@(pkg-config --cflags-only-I dbus-1 2>/dev/null | sed "s/-I//g")',
							'<!@(pkg-config --cflags-only-I gio-2.0 2>/dev/null | sed "s/-I//g")',
						],

						'defines':
                        [
                            # We use some features that are behind config macros in old versions of Pango
                            'PANGO_ENABLE_BACKEND',
                            'PANGO_ENABLE_ENGINE',
						],

						'link_settings':
						{
							# GYP 'libraries' propagates -l flags from static libs to
							# the final linker; 'ldflags' is for -Wl,... options only.
							'libraries':
							[
								'<!@(pkg-config --libs gtk+-3.0 gtk+-unix-print-3.0 2>/dev/null)',
							],
						},

						'dependencies':
						[
							'kernel.gyp:kernel_create_linux_stubs',
						],

						'sources':
						[
							'<(SHARED_INTERMEDIATE_DIR)/src/linux.stubs.cpp',
						],
					},
				],
				[
					'OS == "android"',
					{
						'sources!':
						[
							# Not yet supported on Android
							'src/mblactivityindicator.cpp',
							'src/mblcamera.cpp',
						],

						'dependencies':
						[
							'../thirdparty/libskia/libskia.gyp:libskia',
							'../thirdparty/libfreetype/libfreetype.gyp:libfreetype',
							'../thirdparty/libharfbuzz/libharfbuzz.gyp:libharfbuzz',
						],

						'link_settings':
						{
							'ldflags':
							[
								# Force the entry point to be included in the output
								'-Wl,--undefined,Java_com_runrev_android_Engine_doCreate',

								# mblandroidlcb.cpp contains nothing other than the LCB Invocation Handler
								# native callback function, so force the symbol to be included as otherwise it
								# will be discarded by the linker because nothing in the file is used statically
								'-Wl,--undefined,Java_com_runrev_android_LCBInvocationHandler_doNativeListenerCallback',
								'-Wl,--undefined,Java_com_hyperxtalk_android_LCBInvocationHandler_doNativeListenerCallback',
							],
						},
					},
				],
				[
					'OS == "emscripten"',
					{
						'sources':
						[
							'<@(engine_minizip_source_files)',
						],

						'dependencies':
						[
							'../thirdparty/libskia/libskia.gyp:libskia',
							'../thirdparty/libfreetype/libfreetype.gyp:libfreetype',
						],
					},
				],
				[
					'OS == "mac"',
					{
						'defines':
						[
							# We want to use the new prototypes for the Objective-C
							# dispatch methods as it helps catch certain errors at
							# compile time rather than run time.
							'OBJC_OLD_DISPATCH_PROTOTYPES=0',
						],

						# On macOS, OpenSSL is statically linked — no revsecurity.dylib
						# exists, so libopenssl_stubs would fail to dlopen it.  Instead,
						# we supply no-op initialise/finalise_weak_link_* stubs here and
						# link libcustomssl.a / libcustomcrypto.a directly via libopenssl.
						'sources':
						[
							'src/openssl3_static_stubs.cpp',
						],

						'dependencies':
						[
							'../prebuilt/libopenssl.gyp:libopenssl',
						],
					},
				],
				[
					'OS != "mac"',
					{
						'dependencies':
						[
							'../thirdparty/libopenssl/libopenssl.gyp:libopenssl_stubs',
						],
					},
				],
			],

			'link_settings':
			{
				'conditions':
				[
					[
						'OS == "mac" or OS == "ios"',
						{
							'libraries':
							[
								'$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
								'$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
								'$(SDKROOT)/System/Library/Frameworks/CoreMedia.framework',
								'$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
								'$(SDKROOT)/System/Library/Frameworks/Security.framework',
								'$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
							],
						},
					],
					[
						'OS == "mac"',
						{
							'libraries':
							[
								'$(SDKROOT)/usr/lib/libcups.dylib',
								'$(SDKROOT)/System/Library/Frameworks/Accelerate.framework',
								'$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework',
								'$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
								'$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
								'$(SDKROOT)/System/Library/Frameworks/MediaToolbox.framework',
								'$(SDKROOT)/System/Library/Frameworks/Quartz.framework',
								'$(SDKROOT)/System/Library/Frameworks/UserNotifications.framework',
							],
						},
					],
					[
						'OS == "mac" and target_sdk != "macosx10.6"',
                        {
							# Adding AVFoundation in the list of libraries does not allow
							# us to weak link it. Only adding the linking flag does the job
							'xcode_settings':
							{
								'OTHER_LDFLAGS':
								[
                                    '-weak_framework AVFoundation',
								]
							},
                        },
                    ],
					[
						# VLC: weakly link libvlc/libvlccore from VLC.app on macOS.
						# Weak linking lets standalones that do not bundle VLC still
						# launch; EnsureVLCInstance() guards every symbol before use.
						'OS == "mac"',
						{
							'xcode_settings':
							{
								'OTHER_LDFLAGS':
								[
									'-weak_library /Applications/VLC.app/Contents/MacOS/lib/libvlc.dylib',
									'-weak_library /Applications/VLC.app/Contents/MacOS/lib/libvlccore.dylib',
								],
							},
						},
					],
					[
						# VLC: link against system libvlc on Linux
						# (install with: sudo apt install libvlc-dev)
						'OS == "linux"',
						{
							'libraries':
							[
								'-lvlc',
							],
						},
					],
					[
						# VLC: delay-load libvlc.dll on Windows so standalones that do
						# not bundle VLC still launch; EnsureVLCInstance() is the only
						# call site and returns false gracefully when symbols are null.
						'OS == "win"',
						{
							'libraries':
							[
								'libvlc.lib',
							],
							'msvs_settings':
							{
								'VCLinkerTool':
								{
									'DelayLoadDLLs': 'libvlc.dll',
								},
							},
						},
					],
                    [
                        'OS == "mac" and target_sdk == "macosx10.6"',
                        {
                            'libraries!':
                            [
								'$(SDKROOT)/System/Library/Frameworks/Accelerate.framework',
                                '$(SDKROOT)/System/Library/Frameworks/AVFoundation.framework',
                                '$(SDKROOT)/System/Library/Frameworks/CoreMedia.framework',
								'$(SDKROOT)/System/Library/Frameworks/MediaToolbox.framework',
                            ],
                        },
                    ],
                    [
                        'OS == "ios" and ("11" not in target_sdk) and ("12" not in target_sdk) and ("13" not in target_sdk)',
                        {
                            'libraries':
                            [
                                '$(SDKROOT)/System/Library/Frameworks/AppTrackingTransparency.framework',
                            ],
                        },
                    ],
					[
						'OS == "ios"',
						{
							'libraries':
							[
								'$(SDKROOT)/System/Library/Frameworks/AddressBook.framework',
								'$(SDKROOT)/System/Library/Frameworks/AddressBookUI.framework',
								'$(SDKROOT)/System/Library/Frameworks/AVFoundation.framework',
								'$(SDKROOT)/System/Library/Frameworks/AVKit.framework',
								'$(SDKROOT)/System/Library/Frameworks/CFNetwork.framework',
								'$(SDKROOT)/System/Library/Frameworks/CoreLocation.framework',
								'$(SDKROOT)/System/Library/Frameworks/CoreMedia.framework',
								'$(SDKROOT)/System/Library/Frameworks/CoreMotion.framework',
								'$(SDKROOT)/System/Library/Frameworks/EventKit.framework',
								'$(SDKROOT)/System/Library/Frameworks/EventKitUI.framework',
								'$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
								'$(SDKROOT)/System/Library/Frameworks/MediaPlayer.framework',
								'$(SDKROOT)/System/Library/Frameworks/MessageUI.framework',
								'$(SDKROOT)/System/Library/Frameworks/OpenGLES.framework',
								'$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
								'$(SDKROOT)/System/Library/Frameworks/StoreKit.framework',
								'$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
							],
						},
					],
					[
						'OS == "linux"',
						{
							'libraries':
							[
								'-ldl',
								'-lpthread',
								'-lcups',
								'-ldbus-1',
								'-lX11',
							],
						},
					],
					[
						'OS == "android"',
						{
							'libraries':
							[
								'-lGLESv3',
								'-lEGL',
								'-ljnigraphics',
								'-llog',
								'-lm',
							],
						},
					],
					[
						'OS == "win"',
						{
							'libraries':
							[
								'-ladvapi32',
								'-lcomdlg32',
								'-lcrypt32',
								'-lgdi32',
								'-liphlpapi',
								'-limm32',
								'-lmscms',
								'-lmsimg32',
								'-lpsapi',
								'-lole32',
								'-loleaut32',
								'-lrpcrt4',
								'-lshell32',
								'-lshlwapi',
								'-lstrmiids',
								'-luser32',
								'-lusp10',
								'-lversion',
								'-lwinmm',
								'-lwinspool',
								'-lws2_32',
							],
						},
					],
				],
			},
		},

		{
			'target_name': 'kernel-java',
			'type': 'none',

			'conditions':
			[
				[
					'OS == "android"',
					{
						'variables':
						{
							'java_classes_dir_name': 'classes_livecode_community',
						},

						# Include the rules for compiling Java
						'includes':
						[
							'../config/java.gypi',
						],

						# A little indirection needed to get INTERMEDIATE_DIR escaped properly
						'intermediate_dir_escaped': '<!(["sh", "-c", "echo $1 | sed -e $2", "echo", "<(INTERMEDIATE_DIR)", "s/\\\\$/\\\\\\$/g"])',

						'sources':
						[
							'<@(engine_aidl_source_files)',

							# Outputs from a rule don't get considered as
							# inputs to another rule in Gyp, unfortunately
							'<!@((for x in <@(engine_aidl_source_files); do echo "<(_intermediate_dir_escaped)/${x}"; done) | sed -e "s/\\.aidl$/\\.java/g")',

							# Some of the Java sources depend on the output
							# from AIDL so they come last
							'<@(engine_java_source_files)',
						],
					}
				]
			],
		},
	],

	'conditions':
	[
		[
			'OS == "linux"',
			{
				'targets':
				[
					{
						'target_name': 'kernel_create_linux_stubs',
						'type': 'none',

						'actions':
						[
							{
								'action_name': 'linux_library_stubs',
								'inputs':
								[
									'../util/weak_stub_maker.pl',
									'src/linux.stubs',
								],
								'outputs':
								[
									'<(SHARED_INTERMEDIATE_DIR)/src/linux.stubs.cpp',
								],
								'action':
								[
									'<@(perl)',
									'../util/weak_stub_maker.pl',
									'src/linux.stubs',
									'<@(_outputs)',
								],
							},
						],
					},
				],
			},
		],
	],
}
