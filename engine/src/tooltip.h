//
// MCTooltip class declarations
//
#ifndef	TOOLTIP_H
#define	TOOLTIP_H

#include "object.h"

typedef MCObjectProxy<MCTooltip>::Handle MCTooltipHandle;

class MCTooltip : public MCStack, public MCMixinObjectHandle<MCTooltip>
{
public:
    
    enum { kMCObjectType = CT_TOOLTIP };
    using MCMixinObjectHandle<MCTooltip>::GetHandle;
    
private:
    
	MCStringRef tip;
	int2 mx;
	int2 my;
	MCCard *card;

	MCFontRef m_font;
    
public:
    
	MCTooltip();
	virtual ~MCTooltip();
	virtual void close(void);
	virtual void timer(MCNameRef mptr, MCParameter *params);
	virtual void render(MCContext *target, const MCRectangle& dirty);

	void mousemove(int2 x, int2 y, MCCard *c);
	void clearmatch(MCCard *c);
	void opentip();
	void closetip();
	void cleartip();
	void settip(MCStringRef p_tip);
	MCStringRef gettip()
	{
		return tip;
	}
    
protected:
    
    virtual MCPlatformControlType getcontroltype();
};
#endif
