//
// MCLiteral class declarations
//
#ifndef	LITERAL_H
#define	LITERAL_H

#include "express.h"

class MCLiteral : public MCExpression
{
	MCValueRef value;
public:
	MCLiteral(MCValueRef v)
	{
		/* UNCHECKED */ value = MCValueRetain(v);
	}
    // Default constructor (needed by hxt_deserialize_body).
    // Initialises to empty string; hxt_deserialize_body replaces value.
    MCLiteral()
    {
        value = MCValueRetain(kMCEmptyString);
    }
	~MCLiteral(void)
	{
		MCValueRelease(value);
	}

    virtual Parse_stat parse(MCScriptPoint &, Boolean the);
    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value);

    // HXT: AST serialization.
    virtual bool hxt_serialize(MCHXTASTWriter &w) const override;
    virtual bool hxt_deserialize_body(MCHXTASTReader &r) override;
};

#endif
