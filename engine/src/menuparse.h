#ifndef MENUITEM_H
#define MENUITEM_H


struct MCMenuItem
{
	int4 depth;
	MCStringRef label;
	bool is_disabled: 1;
	bool is_radio: 1;
	bool is_hilited: 1;
    // SN-2014-07-29: [[ Bug 12998 ]] has_tag member put back
    bool has_tag: 1;
	uint4 accelerator;
	MCStringRef accelerator_name;
	uint1 modifiers;
	uint4 mnemonic;
	MCStringRef tag;
	// SF Symbol name set via the !i:<name> flag (empty string = no icon).
	MCStringRef icon;
	uint1 menumode;

	MCMenuItem()
	{
		depth = 0;
		label = MCValueRetain(kMCEmptyString);
		is_disabled = 0;
		is_radio = 0;
		is_hilited = 0;
		accelerator = 0;
		accelerator_name = MCValueRetain(kMCEmptyString);
		modifiers = 0;
		mnemonic = 0;
		tag = MCValueRetain(kMCEmptyString);
        // SN-2014-07-29: [[ Bug 12998 ]] has_tag member put back
        has_tag = false;
		icon = MCValueRetain(kMCEmptyString);
		menumode = 0;
	}

	~MCMenuItem()
	{
		MCValueRelease(label);
		MCValueRelease(accelerator_name);
		MCValueRelease(tag);
		MCValueRelease(icon);
	}
	
	void assignFrom(MCMenuItem *p_from);
};

class IParseMenuCallback
{
public:
	virtual bool Start() {return false;}
	virtual bool ProcessItem(MCMenuItem *p_menuitem) = 0;
	virtual bool End(bool p_has_tags) {return false;}
};

extern void MCParseMenuString(MCStringRef p_string, IParseMenuCallback *p_callback, uint1 p_menumode);
extern uint4 MCLookupAcceleratorKeysym(MCStringRef p_name);
extern const char *MCLookupAcceleratorName(uint4 p_keysym);

#endif
