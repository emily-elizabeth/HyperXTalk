# Software Bill of Materials — HyperXTalk

**Document version:** 1.0  
**Created:** 2026-11-10  
**Author:** Emily-Elizabeth Howard  
**SBOM format:** NTIA minimum elements (Markdown table)  
**SPDX document namespace:** https://hyperxtalk.com/sbom/v0.9.16

---

## Primary Component

| Field | Value |
|---|---|
| Name | HyperXTalk |
| Version | 0.9.16 (build 15882) |
| Supplier | Emily-Elizabeth Howard |
| License | GPL-3.0-only |
| Source | https://github.com/OpenXTalk-org/OpenXTalk_Community_DPE |
| Homepage | https://hyperxtalk.com |
| Description | Cross-platform scripting environment inspired by HyperCard and LiveCode, combining a HyperTalk-derived scripting language with native API access on macOS, Windows, and Linux. |

---

## Third-Party Components

### C/C++ Libraries (bundled in `/thirdparty/`)

| Name | Version | Supplier | License | Source URL |
|---|---|---|---|---|
| cairo | 1.18.4 | cairographics.org | LGPL-2.1-or-later OR MPL-1.1 | https://cairographics.org/ |
| curl / libcurl | 8.19.0 | Daniel Stenberg et al. | curl (MIT-like) | https://curl.se/libcurl/ |
| Expat | 2.6.4 | Expat maintainers | MIT | https://libexpat.github.io/ |
| libffi | git snapshot | libffi contributors | MIT | https://github.com/libffi/libffi |
| FreeType | 2.10.4 | FreeType Project | FreeType License (BSD-style) OR GPL-2.0-only | https://freetype.org/ |
| giflib | 5.1.4 | giflib contributors | MIT | https://sourceforge.net/projects/giflib/ |
| HarfBuzz | 0.9.28 | HarfBuzz contributors | MIT (Old MIT) | https://harfbuzz.github.io/ |
| iODBC | 3.52.16 | OpenLink Software | LGPL-2.0-or-later OR BSD-3-Clause | https://github.com/openlink/iODBC |
| libjpeg | 9b | Independent JPEG Group | IJG License | http://www.ijg.org/ |
| libmysqlclient | 9.6.0 | Oracle Corporation | GPL-2.0-only | https://dev.mysql.com/ |
| OpenSSL (shim) | see Prebuilt | OpenSSL Project | Apache-2.0 | https://openssl.org/ |
| PCRE | 8.39 | Philip Hazel / PCRE maintainers | BSD-3-Clause | https://www.pcre.org/ |
| libpng | 1.6.46 | PNG Reference Library contributors | PNG License (zlib/libpng) | http://libpng.org/ |
| libpq (PostgreSQL C client) | 18.3 | PostgreSQL Global Dev. Group | PostgreSQL License | https://www.postgresql.org/ |
| Skia | commit 20471894 | Google LLC | BSD-3-Clause | https://skia.org/ |
| SQLite | 3.34.0 | D. Richard Hipp | Public Domain | https://sqlite.org/ |
| VLC / libvlc | 3.0.23 | VideoLAN Project | LGPL-2.1-or-later | https://www.videolan.org/ |
| libwebp | 1.4.0 | Google LLC | BSD-3-Clause | https://chromium.googlesource.com/webm/libwebp |
| libxml2 | 2.15.3 | GNOME Project / Daniel Veillard | MIT | https://gitlab.gnome.org/GNOME/libxml2 |
| libxslt | 1.1.45 | GNOME Project / Daniel Veillard | MIT | https://gitlab.gnome.org/GNOME/libxslt |
| zlib | 1.2.3 | Jean-loup Gailly & Mark Adler | zlib License | https://zlib.net/ |
| libzip | 1.11.4 | Dieter Baron & Thomas Klausner | BSD-3-Clause | https://libzip.org/ |

### Prebuilt Binaries (`/prebuilt/build/`)

| Name | Version | Supplier | License | Notes |
|---|---|---|---|---|
| OpenSSL | 1.1.1g | OpenSSL Project | OpenSSL License (dual SSLeay) | macOS arm64 legacy; superseded by 3.x |
| OpenSSL | 3.4.1 | OpenSSL Project | Apache-2.0 | macOS arm64 and x86_64 |
| OpenSSL | 3.6.2 | OpenSSL Project | Apache-2.0 | macOS arm64 and x86_64 (current) |
| ICU | 58.2 | Unicode Consortium / IBM | ICU License (MIT-like) | macOS arm64; International Components for Unicode |

### Windows SDK Package (`/packages/`)

| Name | Version | Supplier | License | Notes |
|---|---|---|---|---|
| Microsoft WebView2 SDK | 1.0.3912.50 | Microsoft Corporation | Microsoft SDK License | Windows browser widget; NuGet package |

### Toolchain (`/toolchain/`)

| Name | Version | Supplier | License | Notes |
|---|---|---|---|---|
| Gentle 97 | 97 Campus Edition | Metarga GmbH | Gentle Campus License | Parser generator used by lc-compile; free for personal/educational use only |
| GYP (pylib) | n/a | Google LLC | Apache-2.0 OR BSD-3-Clause | Build system used by the pylib/packaging component |

### IDE Extensions (`/ide/Ext/`)

| Name | Version | Supplier | License | Notes |
|---|---|---|---|---|
| mergJSON | 1.0.70 | mergExt / OpenXTalk | GPL-3.0-only | JSON library for IDE use |

### Test Framework (`/libcpptest/googletest/`)

| Name | Version | Supplier | License | Notes |
|---|---|---|---|---|
| Google Test | snapshot | Google LLC | BSD-3-Clause | C++ unit testing framework |
| Google Mock | snapshot | Google LLC | BSD-3-Clause | C++ mocking framework; bundled with Google Test |

### LCB Extensions (bundled, `/extensions/` and `/mac-bin/packaged_extensions/`)

These are first-party or LiveCode-community extensions written in LiveCode Builder (LCB), distributed under the same GPL-3.0-only license as HyperXTalk unless noted otherwise.

| Extension ID | Description |
|---|---|
| com.livecode.library.canvas | Canvas 2D drawing library |
| com.livecode.library.iconsvg | SVG icon library |
| com.livecode.library.ini | INI file read/write |
| com.livecode.library.native.mac.statusmenu | macOS menu bar status item |
| com.livecode.library.objectrepository | Object persistence library |
| com.livecode.library.scriptitems | Script editor support items |
| com.livecode.library.timezone | Timezone conversion library |
| com.livecode.library.widgetutils | Widget utility helpers |
| com.livecode.widget.browser | Web view widget |
| com.livecode.widget.checkbox | Checkbox widget |
| com.livecode.widget.clock | Analog clock widget |
| com.livecode.widget.colorswatch | Color swatch picker |
| com.livecode.widget.gradientrampeditor | Gradient ramp editor |
| com.livecode.widget.headerbar | Header bar widget |
| com.livecode.widget.iconpicker | Icon picker widget |
| com.livecode.widget.linegraph | Line graph widget |
| com.livecode.widget.native.mac.button | macOS native button |
| com.livecode.widget.native.mac.textfield | macOS native text field |
| com.livecode.widget.navbar | Navigation bar widget |
| com.livecode.widget.paletteactions | Palette actions widget |
| com.livecode.widget.progressbar | Progress bar widget |
| com.livecode.widget.pushbutton | Push button widget |
| com.livecode.widget.radiobutton | Radio button widget |
| com.livecode.widget.segmented | Segmented control widget |
| com.livecode.widget.selector | Selector widget |
| com.livecode.widget.spinner | Spinner/activity indicator widget |
| com.livecode.widget.svgpath | SVG path rendering widget |
| com.livecode.widget.switchbutton | Toggle switch widget |
| com.livecode.widget.tile | Tile/card widget |
| com.livecode.widget.treeview | Tree view widget |
| com.hyperxtalk.library.json | JSON library (HyperXTalk-specific) |
| com.hyperxtalk.library.macscrollenhancement | macOS scroll enhancement |
| com.hyperxtalk.macoswindowresizeanimation | macOS window resize animation |
| community.livecode.livecode.list | Community list widget |
| com.livecode.extensions.livecode.chart | Chart widget |

---

## License Summary

| SPDX Identifier | Components |
|---|---|
| GPL-3.0-only | HyperXTalk (primary), mergJSON |
| GPL-2.0-only | libmysqlclient |
| LGPL-2.1-or-later | cairo, iODBC, libvlc |
| MPL-1.1 | cairo (alternative) |
| Apache-2.0 | OpenSSL 3.x, GYP |
| OpenSSL License | OpenSSL 1.1.1g (dual SSLeay/OpenSSL — pre-Apache era) |
| MIT / MIT-like | Expat, libffi, giflib, HarfBuzz, libxml2, libxslt, curl, ICU |
| BSD-3-Clause | PCRE, Skia, libwebp, libzip, Google Test/Mock, iODBC (alt) |
| FreeType License | FreeType (alternative: GPL-2.0) |
| IJG License | libjpeg |
| PNG License (zlib/libpng) | libpng |
| PostgreSQL License | libpq |
| Public Domain | SQLite |
| zlib License | zlib |
| ICU License | ICU |
| Microsoft SDK License | WebView2 |
| Gentle Campus License | Gentle 97 (non-commercial/educational only) |

---

## Notes

1. **Gentle 97**: The Campus Edition license restricts use to personal and educational purposes. Commercial redistribution of HyperXTalk binaries may require a separate commercial license from Metarga GmbH, or replacement of the parser generator.

2. **libmysqlclient (GPL-2.0)**: MySQL's client library is GPL-licensed. Linking it into HyperXTalk is consistent with HyperXTalk's own GPL-3.0 license, but distributors should be aware that MySQL 8.0+ is covered by GPL-2.0 (which is compatible with GPL-3.0 downstream, but requires distribution of source or a GPL exception from Oracle for proprietary use).

3. **OpenSSL 1.1.1g**: This version uses the old dual OpenSSL/SSLeay license rather than Apache-2.0. It has reached end-of-life (September 2023) and should be replaced. The 3.x prebuilts are current.

4. **libffi**: Exact version not recorded in the repository; the `src/` tree appears to be a git snapshot from the libffi main branch. Consider pinning to a tagged release.

5. **VLC / libvlc 3.0.23**: Only the Windows (`win64`) prebuilt is present in the repository. macOS and Linux builds rely on system or separately installed VLC.

6. **Skia**: Referenced by git commit `20471894eaa441193d5ae8f2395e8244c91c55af` rather than a versioned release. Date of that commit should be recorded if the exact Skia version matters for CVE tracking.

7. **HarfBuzz 0.9.28**: This is a very old version (circa 2013). Upstream has long since moved to 8.x. Consider upgrading, particularly for font shaping correctness and security.

8. **zlib 1.2.3**: Shipped version is significantly outdated (current is 1.3.x). zlib 1.2.11+ includes important security fixes; upgrade is recommended.
