//
// MCHandler class declarations
//
#ifndef	STATEMENT_H
#define	STATEMENT_H

// Self-contained type dependencies — allows statemnt.h to be included from
// any translation unit without requiring the caller to include these first.
#include "parsedef.h"    // Parse_stat, Functions; transitively: typedefs.h (uint2, uint4, Boolean)

class MCScriptPoint;
class MCParameter;
class MCChunk;
class MCExpression;
class MCVarref;
class MCHandler;
class MCHXTASTWriter;
class MCHXTASTReader;
class MCExecContext;

class MCStatement
{
protected:
	uint2 line;
	uint2 pos;
	MCStatement *next;
public:
	MCStatement();

	virtual ~MCStatement();
	virtual Parse_stat parse(MCScriptPoint &);
	virtual void exec_ctxt(MCExecContext&);

	virtual uint4 linecount();

    // HXT: AST serialization.
    //
    // hxt_serialize() writes this statement's type-specific members into w.
    // The caller (MCHandler::hxt_serialize) has already written the common
    // statement header (stmt_type, line, pos) before calling this method.
    // Returns false on error.
    //
    // hxt_deserialize() is a static factory: reads the common header from r,
    // allocates the right MCStatement subclass via MCN_new_statement(), then
    // calls the subclass hxt_deserialize_body() to fill in its members.
    // Returns nullptr on error.
    virtual bool hxt_serialize(MCHXTASTWriter &w) const;
    static MCStatement *hxt_deserialize(MCHXTASTReader &r);

    // Called by the static factory after allocating the subclass instance.
    // Reads type-specific members from r.  The common header has already been
    // consumed.  Returns false on error.
    virtual bool hxt_deserialize_body(MCHXTASTReader &r);
	
	void setnext(MCStatement *n)
	{
		next = n;
	}
	MCStatement *getnext()
	{
		return next;
	}
	uint4 countlines(MCStatement *stmp);
	void deletestatements(MCStatement *start);
	void deletetargets(MCChunk **targets);
	Parse_stat gettargets(MCScriptPoint &, MCChunk **targets, Boolean forset);
	Parse_stat getparams(MCScriptPoint &, MCParameter **params);
	Parse_stat getmods(MCScriptPoint &, uint2 &mstate);
	Parse_stat gettime(MCScriptPoint &sp, MCExpression **in, Functions &units);
	void initpoint(MCScriptPoint &);
	uint2 getline()
	{
		return line;
	}
	uint2 getpos()
	{
		return pos;
	}
};

#endif
