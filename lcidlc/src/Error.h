#ifndef __ERROR__
#define __ERROR__

#include "foundation.h"

#ifndef __VALUE__
#include "Value.h"
#endif

////////////////////////////////////////////////////////////////////////////////

enum
{
	kErrorNone,
	kErrorCouldNotOpenFile,
	kErrorCouldNotReadFile,
	kErrorCantAdvancePastEnd,
	kErrorCantRetreatPastMark,
	kErrorNoCurrentToken
};

bool Throw(uint32_t error);
bool ThrowWithHint(uint32_t error, ValueRef hint);

////////////////////////////////////////////////////////////////////////////////

#endif
