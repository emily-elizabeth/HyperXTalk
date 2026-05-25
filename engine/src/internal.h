#ifndef __INTERNAL_H
#define __INTERNAL_H

#include "statemnt.h"

////////////////////////////////////////////////////////////////////////////////

class MCScriptPoint;
class MCStatement;

////////////////////////////////////////////////////////////////////////////////

struct MCInternalVerbInfo
{
	const char *first_token;
	const char *second_token;
	MCStatement *(*factory)(void);
};

class MCInternal: public MCStatement
{
	MCStatement *f_statement;

public:
	MCInternal(void);

	virtual ~MCInternal(void);
	virtual Parse_stat parse(MCScriptPoint&);
    virtual void exec_ctxt(MCExecContext & ctxt);
};

inline MCInternal::MCInternal(void)
  : f_statement(NULL)
{
}

////////////////////////////////////////////////////////////////////////////////

#endif
