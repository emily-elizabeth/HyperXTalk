#include "dbdriver.h"
#ifdef MACOS
#include <ctype.h>
#endif

const char DBNullValue[] = "";


char *longtostring(long inValue)
{
	static char numstring[32];
	sprintf(numstring, "%lu", inValue );
	return numstring;
}





void DBList::add(DBObject *newdbnode) {dblist.push_back(newdbnode);}

int DBList::getsize() {return dblist.size();}

void DBList::clear() 
{
	if (dblist.empty()) 
		return;
	DBObjectList::iterator theIterator;
	for (theIterator = dblist.begin(); theIterator != dblist.end(); theIterator++){
		DBObject *curobject = (DBObject *)(*theIterator);
		delete curobject;
	}
	dblist.clear();
}

Bool DBList::erase(const unsigned int fid)
{
	DBObjectList::iterator theIterator;
	for (theIterator = dblist.begin(); theIterator != dblist.end(); theIterator++){
		DBObject *curobject = (DBObject *)(*theIterator);
		if (curobject->GetID() == fid){
			delete curobject;
			dblist.erase(theIterator);
			return True;
		}
	}
	return False;
}


DBObject *DBList::findIndex(const int tindex)
{
	int i = 0;
	DBObjectList::iterator theIterator;
	for (theIterator = dblist.begin(); theIterator != dblist.end(); theIterator++){
		if (i++ == tindex){
			DBObject *curobject = (DBObject *)(*theIterator);
			return curobject;
		}
	}
	return NULL;
}

DBObject *DBList::find(const unsigned int fid)
{
	DBObjectList::iterator theIterator;
	for (theIterator = dblist.begin(); theIterator != dblist.end(); theIterator++){
		DBObject *curobject = (DBObject *)(*theIterator);
		if (curobject->GetID() == fid)
			return curobject;
	}		
	return NULL;
}


DBList::~DBList() {clear();}

DBObjectList *DBList::getList() {return &dblist;}

#ifdef MIN
#undef MIN
#endif
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

