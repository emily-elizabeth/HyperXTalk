#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "parsedef.h"
#include "objdefs.h"


#include "object.h"
#include "undolst.h"

#include "field.h"
#include "paragraf.h"

#include "globals.h"

MCUndonode::MCUndonode(MCObject *objptr, Ustruct *us)
{
	object = objptr;
	savedata = us;
}

MCUndonode::~MCUndonode()
{
	object->freeundo(savedata);
	switch (savedata->type)
	{
	case UT_DELETE:
		delete object;
		break;
	case UT_DELETE_TEXT:
	case UT_REPLACE_TEXT:
	case UT_TYPE_TEXT:
	case UT_MOVE_TEXT:
		{
			MCParagraph *pgptr = (MCParagraph *)savedata->ud.text.data;
			while (pgptr != NULL)
			{
				MCParagraph *tpgptr = pgptr->remove
				                      (pgptr);
				delete tpgptr;
			}
		}
		break;
	default:
		break;
	}
	delete savedata;
}

void MCUndonode::undo()
{
	object->undo(savedata);
}

MCUndolist::MCUndolist()
{
	nodes = NULL;
}

MCUndolist::~MCUndolist()
{
	while (nodes != NULL)
	{
		MCUndonode *uptr = nodes->remove
		                   (nodes);
		delete uptr;
	}
}

void MCUndolist::savestate(MCObject *objptr, Ustruct *us)
{
	MCUndonode *uptr = new (nothrow) MCUndonode(objptr, us);
	uptr->appendto(nodes);
    if (MCdefaultstackptr)
        MCdefaultstackptr->getcurcard()->message(MCM_undo_changed);
}

void MCUndolist::freestate()
{
	while (nodes != NULL)
	{
		MCUndonode *uptr = nodes->remove
		                   (nodes);
		delete uptr;
	}
}

void MCUndolist::freeobject(MCObject *objptr)
{
	if (nodes != NULL)
	{
		MCUndonode *uptr = nodes;
		do
		{
			if (uptr->getobject() == objptr)
			{
				uptr->remove
				(nodes);
				delete uptr;
				return;
			}
			uptr = uptr->next();
		}
		while (uptr != nodes);
	}
}

MCObject *MCUndolist::getobject()
{
	if (nodes == NULL)
		return NULL;
	return nodes->getobject();
}

Ustruct *MCUndolist::getstate()
{
	if (nodes == NULL)
		return NULL;
	return nodes->getdata();
}

Boolean MCUndolist::undo()
{
	if (nodes != NULL)
	{
		MCUndonode *t_new_nodes;
		t_new_nodes = NULL;

		do
		{
			MCUndonode *t_current;
			t_current = nodes;
			nodes -> remove(nodes);

			t_current -> undo();
			t_current -> insertto(t_new_nodes);
		}
		while (nodes != NULL);
		nodes = t_new_nodes;
		return True;
	}
	return False;
}
