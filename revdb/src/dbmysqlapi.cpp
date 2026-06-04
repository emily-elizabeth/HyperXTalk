#include "dbmysql.h"

#if defined(_WINDOWS)
#define LIBRARY_EXPORT __declspec(dllexport)
#else
#define LIBRARY_EXPORT __attribute__((__visibility__("default")))
#endif

unsigned int *DBObject::idcounter = NULL;

extern "C" LIBRARY_EXPORT DBConnection *newdbconnectionref() 
{
	DBConnection *ref = new (nothrow) DBConnection_MYSQL();
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

extern "C" LIBRARY_EXPORT void setcallbacksref(DBcallbacks *callbacks);

////////////////////////////////////////////////////////////////////////////////

// The static function export table for iOS external linkage requirements.

#ifdef TARGET_SUBPLATFORM_IPHONE
extern "C" {
	
	struct LibExport
	{
		const char *name;
		void *address;
	};
	
	struct LibInfo
	{
		const char **name;
		struct LibExport *exports;
	};
	
	static const char *__libname = "dbmysql";
	
	static struct LibExport __libexports[] =
	{
		{ "newdbconnectionref", (void *)newdbconnectionref },
		{ "releasedbconnectionref", (void *)releasedbconnectionref },
		{ "setidcounterref", (void *)setidcounterref },
		{ "setcallbacksref", (void *)setcallbacksref },
		{ 0, 0 }
	};
	
	struct LibInfo __libinfo =
	{
		&__libname,
		__libexports
	};
	
	__attribute((section("__DATA,__libs"))) volatile struct LibInfo *__libinfoptr_dbmysql __attribute__((__visibility__("default"))) = &__libinfo;
}
#endif

////////////////////////////////////////////////////////////////////////////////
