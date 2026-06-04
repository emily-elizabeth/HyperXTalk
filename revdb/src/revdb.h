#include "dbdriver.h"

typedef DBConnection *(*new_connectionrefptr) ();
typedef void (*release_connectionrefptr) (DBConnection *dbref);
typedef void (*idcounterrefptr) (unsigned int *tidcounter);
typedef void (*set_callbacksrefptr)(const DBcallbacks *callbacks);

struct DATABASEREC
{
    char dbname[255];
    idcounterrefptr idcounterptr;
    new_connectionrefptr  newconnectionptr;
    release_connectionrefptr releaseconnectionptr;
    set_callbacksrefptr setcallbacksptr;
    void *driverref;
};

typedef vector<DATABASEREC *> DATABASERECList;
