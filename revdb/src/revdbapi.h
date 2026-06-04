#define REVDB_EXPORTS 1
#ifdef REVDB_EXPORTS
#ifdef X11
#define REVDB_API
#endif
#ifdef WIN32
#define REVDB_API __declspec(dllexport)
#endif
#ifdef MACOS 
#ifdef MACHO
#define REVDB_API
#else
#define REVDB_API __declspec(dllexport)
#endif
#endif
#endif

#include "database.h"


#ifdef __cplusplus
extern "C" {
#endif
REVDB_API DBConnection *newdbconnectionref();
REVDB_API void releasedbconnectionref(DBConnection *dbref);
REVDB_API void setidcounterref(unsigned int *tidcounter);
#ifdef __cplusplus
}
#endif
