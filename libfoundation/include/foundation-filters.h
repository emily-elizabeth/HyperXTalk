#ifndef __MC_FOUNDATION_FILTERS__
#define __MC_FOUNDATION_FILTERS__

#ifndef __MC_FOUNDATION__
#include <foundation.h>
#endif

////////////////////////////////////////////////////////////////////////////////

bool MCFiltersBase64Decode(MCStringRef p_src, MCDataRef& r_dst);
bool MCFiltersBase64Encode(MCDataRef p_src, MCStringRef& r_dst);
bool MCFiltersCompress(MCDataRef p_source, MCDataRef& r_result);
bool MCFiltersIsCompressed(MCDataRef p_source);
bool MCFiltersDecompress(MCDataRef p_source, MCDataRef& r_result);
bool MCFiltersIsoToMac(MCDataRef p_source, MCDataRef &r_result);
bool MCFiltersMacToIso(MCDataRef p_source, MCDataRef &r_result);
bool MCFiltersUrlEncode(MCStringRef p_source, MCStringRef& r_result);
bool MCFiltersUrlDecode(MCStringRef p_source, MCStringRef& r_result);

////////////////////////////////////////////////////////////////////////////////

#endif
