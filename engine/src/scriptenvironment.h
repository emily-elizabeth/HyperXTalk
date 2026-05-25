#ifndef __MC_SCRIPT_ENVIRONMENT__
#define __MC_SCRIPT_ENVIRONMENT__

typedef char *(*MCScriptEnvironmentCallback)(const char* const* p_arguments, unsigned int p_argument_count);

class MCScriptEnvironment
{
public:
	virtual ~MCScriptEnvironment() {}

	virtual void Retain(void) = 0;
	virtual void Release(void) = 0;

	virtual bool Define(const char *p_function, MCScriptEnvironmentCallback p_callback) = 0;

	virtual void Run(MCStringRef p_script, MCStringRef& r_output) = 0;

	virtual char *Call(const char *p_method, const char** p_arguments, unsigned int p_argument_count) = 0;
};

#endif
