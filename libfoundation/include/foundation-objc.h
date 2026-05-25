#ifndef __MC_FOUNDATION_OBJC__
#define __MC_FOUNDATION_OBJC__

#ifndef __MC_FOUNDATION__
#include <foundation.h>
#endif

#ifdef __OBJC__
#import <Foundation/NSString.h>
#import <Foundation/NSData.h>

NSString *MCStringConvertToAutoreleasedNSString(MCStringRef string);
NSString *MCNameConvertToAutoreleasedNSString(MCNameRef name);
NSData *MCDataConvertToAutoreleasedNSData(MCDataRef data);
#endif

extern MCTypeInfoRef kMCObjcDelegateCallbackSignatureErrorTypeInfo;
extern MCTypeInfoRef kMCObjcDelegateMappingErrorTypeInfo;
#endif
