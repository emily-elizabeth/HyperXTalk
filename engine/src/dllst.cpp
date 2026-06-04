#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"

#include "dllst.h"

MCDLlist::~MCDLlist()
{
	nptr->pptr = pptr;
	pptr->nptr = nptr;
}

void MCDLlist::removelink(MCObject *optr)
{}

void MCDLlist::totop(MCDLlist *&list)
{
	if (this != list)
	{
		remove(list);
		insertto(list);
	}
}

void MCDLlist::appendto(MCDLlist *&list)
{
	if (list == NULL)
		list = this;
	else
	{
		list->pptr->nptr = this;
		pptr->nptr = list;
		MCDLlist *tptr = list->pptr;
		list->pptr = pptr;
		pptr = tptr;
	}
}

void MCDLlist::insertto(MCDLlist *&list)
{
	appendto(list);
	list = this;
}

void MCDLlist::append(MCDLlist *node)
{
	node->pptr->nptr = nptr;
	nptr->pptr = node->pptr;
	node->pptr = this;
	nptr = node;
}

void MCDLlist::splitat(MCDLlist *node)
{
	pptr->nptr = node;
	node->pptr->nptr = this;
	MCDLlist *tptr = node->pptr;
	node->pptr = pptr;
	pptr = tptr;
}

MCDLlist *MCDLlist::remove(MCDLlist *&list)
{
	if (list == this)
	{
		if (list->nptr == this)
			list = NULL;
		else
			list = nptr;
	}
	nptr->pptr = pptr;
	pptr->nptr = nptr;
	pptr = nptr = this;
	return this;
}

#ifdef  _DEBUG_MALLOC_INC
void MCDLlist::verify(char *where)
{
	MCDLlist *ptr = this;
	do
	{
		if (ptr->nptr->pptr != ptr)
			fprintf(stderr, "%s list is bad at %d, this is %d\n",
			        where, (int)ptr, (int)this);
		ptr = ptr->nptr;
	}
	while (ptr != this);
}
#endif
