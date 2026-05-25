#include "foundation.h"

MC_DLLEXPORT int platform_main(int argc, char *argv[], char *envp[]);


MC_DLLEXPORT_DEF int main(int argc, char *argv[], char *envp[])
{
	return platform_main(argc, argv, envp);
}
