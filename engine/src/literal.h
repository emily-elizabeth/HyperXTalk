/* Copyright (C) 2003-2015 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

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
