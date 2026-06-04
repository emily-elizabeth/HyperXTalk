#include "prefix.h"

#include "module-resources.h"
#include "libscript/script.h"

#include "filepath.h"
#include "foundation-auto.h"

extern bool MCEngineLookupResourcePathForModule(MCScriptModuleRef p_module, MCStringRef &r_resource_path);

bool MCResourceResolvePath(MCStringRef p_name, MCStringRef &r_path)
{
	MCScriptModuleRef t_module;
	t_module = MCScriptGetCurrentModule();
	if (t_module == nil)
		return false;
	
	MCAutoStringRef t_path;
	if (!MCEngineLookupResourcePathForModule(t_module, &t_path))
		return false;
	
	return MCPathAppend(*t_path, p_name, r_path);
}
