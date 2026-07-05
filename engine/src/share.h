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

#ifndef SHARE_H
#define SHARE_H

#include "statemnt.h"

enum MCShareItemType
{
    kMCShareText,
    kMCShareFile,
    kMCShareImage,
};

class MCShare : public MCStatement
{
    MCShareItemType  m_share_type;
    MCExpression    *m_data;
    MCExpression    *m_from;
    bool             m_from_toolbar; // true → m_from is a toolbar item name

public:
    MCShare();
    virtual ~MCShare();
    virtual Parse_stat parse(MCScriptPoint &sp);
    virtual void exec_ctxt(MCExecContext &ctxt);
};

#endif // SHARE_H
