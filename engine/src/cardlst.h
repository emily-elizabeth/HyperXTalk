#ifndef	CARDLIST_H
#define	CARDLIST_H

#include "dllst.h"
#include "card.h"

#define MAX_FILL 32
#define MIN_FILL 32

class MCCardnode : public MCDLlist
{
public:
	MCCardHandle card;
	MCCardnode()
	{ }
	~MCCardnode();
	MCCardnode *next()
	{
		return (MCCardnode *)MCDLlist::next();
	}
	MCCardnode *prev()
	{
		return (MCCardnode *)MCDLlist::prev();
	}
	void totop(MCCardnode *&list)
	{
		MCDLlist::totop((MCDLlist *&)list);
	}
	void insertto(MCCardnode *&list)
	{
		MCDLlist::insertto((MCDLlist *&)list);
	}
	void appendto(MCCardnode *&list)
	{
		MCDLlist::appendto((MCDLlist *&)list);
	}
	void append(MCCardnode *node)
	{
		MCDLlist::append((MCDLlist *)node);
	}
	void splitat(MCCardnode *node)
	{
		MCDLlist::splitat((MCDLlist *)node) ;
	}
	MCCardnode *remove
	(MCCardnode *&list)
	{
		return (MCCardnode *)MCDLlist::remove
			       ((MCDLlist *&)list);
	}
};

class MCCardlist
{
	MCCardnode *cards;
	MCCardnode *first;
	uint2 interval;
public:
	MCCardlist();
	~MCCardlist();
	void trim();
	bool GetRecent(MCExecContext& ctxt, MCStack *stack, Properties which, MCStringRef& r_props);
	void addcard(MCCard *cptr);
	void deletecard(MCCard *cptr);
	void deletestack(MCStack *cptr);
	void gorel(int2 offset);
	MCCard *getrel(int2 offset);
	void godirect(Boolean start);
	void pushcard(MCCard *cptr);
	MCCard *popcard();
};
#endif
