# Steps for updating CEF

- Go to http://opensource.spotify.com/cefbuilds/index.html
- Download the latest standard distribution for Windows and Linux and extract
- Delete `thirdparty/libcef/include` and `thirdparty/libcef/libcef_dll`
- Copy `include` and `libcef_dll` from the CEF windows build to
`thirdparty/libcef`
- Execute `python gen_libcef_paths.py libcef_dll > libcef.gypi`
- Execute `python gen_libcef_stubs.py include > libcef.stubs`
- Stderr will list unknown parameter or return types so edit `gen_libcef_stubs.py`
to add them and run again
- Copy `include` from the CEF linux build to `thirdparty/libcef` to merge  in
the extra headers required for linux. Note this is done after the stubs are
generated so we don't include stubs for anything linux specific which seems to
be `cef_get_xdisplay` at the moment.
- CD to `thirdparty/libcef`
- Copy binaries and resources from the CEF build to the prebuilt folders checking
for any new file names to update in the gyp files and prebuilt builder. Note copy
the binaries from the Release folder even to the debug prebuilt folders.
- Reconfigure and build
- Fix any API changes and make sure things work
- Update `prebuilt/versions/cef` and `prebuilt/versions/cef_buildrevision`
- Push up a patch to livecode and thirdparty
- Kick off prebuilts
