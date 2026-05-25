#include "prefix.h"

#include "parsedef.h"
#include "filedefs.h"

#include "dispatch.h"
#include "stacksecurity.h"
#include "globals.h"
#include "deploy.h"

Exec_stat
MCDeployToEmscripten(const MCDeployParameters & p_params)
{
	bool t_success = true;

	/* Open the output file */
	MCDeployFileRef t_output = nil;
	if (t_success && !MCDeployFileOpen(p_params . output, kMCOpenFileModeCreate, t_output))
		t_success = MCDeployThrow(kMCDeployErrorNoOutput);

	uint32_t t_project_size = 0;
	/* Write the stack capsule data */
	if (t_success)
		t_success = MCDeployWriteProject(p_params, false, t_output, 0, t_project_size);
	
	MCDeployFileClose(t_output);
	
	return t_success ? ES_NORMAL : ES_ERROR;
}

