//
// Visual effect class declarations
//
#ifndef	VISUAL_H
#define	VISUAL_H

#include "statemnt.h"

class MCVisualEffect : public MCStatement
{
	struct KeyValue
	{
		KeyValue *next;
		const char *key;
		bool has_id;
		MCExpression *value;
	};

	MCExpression *nameexp;
	MCExpression *soundexp;
	KeyValue *parameters;
	
	Visual_effects effect;
	Visual_effects direction;
	Visual_effects speed;
	Visual_effects image;
public:
	MCVisualEffect()
	{
		nameexp = soundexp = NULL;
		parameters = NULL;
		image = VE_CARD;
		effect = direction = VE_UNDEFINED;
		speed = VE_NORMAL;
	}
	virtual ~MCVisualEffect();
	virtual Parse_stat parse(MCScriptPoint &);
    virtual void exec_ctxt(MCExecContext &ctxt);
};

struct MCEffectArgument
{
	MCEffectArgument *next;
	MCStringRef key;
	MCStringRef value;
};

class MCEffectList
{
public:
	MCEffectList *next;
	MCStringRef name;
	MCStringRef sound;
	MCEffectArgument *arguments;
	
	Visual_effects type;
	Visual_effects direction;
	Visual_effects speed;
	Visual_effects image;
	
	MCEffectList()
	{
		arguments = NULL;
		next = NULL;
		name = sound = NULL;
		type = direction = speed = image = VE_UNDEFINED;
	}
	~MCEffectList();
	MCEffectList *getnext()
	{
		return next;
	}
};
#endif
