#include "prefix.h"

#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "deploy.h"

////////////////////////////////////////////////////////////////////////////////

bool MCDeploySecuritySecureStandalone(void *p_file, uint32_t p_start_offset, uint32_t p_amount, uint32_t& x_offset, uint8_t *p_digest)
{
	bool t_success;
	t_success = true;
	
	if (t_success)
	{
		uint32_t t_zero;
		t_zero = 0;
		x_offset = (x_offset + 3) & ~3;
		t_success = MCDeployFileWriteAt((MCDeployFileRef)p_file, &t_zero, sizeof(uint32_t), x_offset);
	}
	
	if (t_success)
		x_offset += sizeof(uint32_t);
	
	return t_success;
}


////////////////////////////////////////////////////////////////////////////////
