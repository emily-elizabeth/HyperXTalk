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
