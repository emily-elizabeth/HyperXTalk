#include "dbpostgresql.h"

#if defined(_WINDOWS)
#define LIBRARY_EXPORT __declspec(dllexport)
#else
#define LIBRARY_EXPORT __attribute__((__visibility__("default")))
#endif

unsigned int *DBObject::idcounter = NULL;

extern "C" LIBRARY_EXPORT DBConnection *newdbconnectionref() 
{
	DBConnection *ref = new (nothrow) DBConnection_POSTGRESQL();
	return ref;
}

extern "C" LIBRARY_EXPORT void releasedbconnectionref(DBConnection *dbref) 
{
	if(dbref) 
		delete dbref;
}

extern "C" LIBRARY_EXPORT void setidcounterref(unsigned int *tidcounter)
{
	DBObject::idcounter = tidcounter;
}
