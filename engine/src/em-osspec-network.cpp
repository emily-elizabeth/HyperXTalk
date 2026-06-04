#include "em-util.h"

#include "sysdefs.h"
#include "osspec.h"

/* ================================================================
 * Socket handling
 * ================================================================ */

MCSocket *
MCS_accept(uint16_t p_port,
           MCObject *p_object,
           MCNameRef p_message,
           Boolean p_datagram,
           Boolean p_secure,
           Boolean p_sslverify,
           MCStringRef p_sslcertfile)
{
	MCEmscriptenNotImplemented();
	return nil;
}

bool
MCS_ha(MCSocket *p_socket,
       MCStringRef & r_address)
{
	MCEmscriptenNotImplemented();
	return false;
}
