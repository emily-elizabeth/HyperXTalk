#ifndef __COMMANDS_ASK_H
#define __COMMANDS_ASK_H

#ifndef __EXECUTION_POINT_H

#endif

#ifndef __VARIABLE_H
#include "variable.h"
#endif

#ifndef __EXPRESSION_H
#include "express.h"
#endif

#ifndef __STATEMENT_H
#include "statemnt.h"
#endif

class MCAsk : public MCStatement
{
	Ask_type mode;
	MCExpression *title;
	Boolean sheet;

	union
	{
		struct
		{
			MCExpression *prompt;
			MCExpression *answer;
			bool hint : 1;
		} question;
		struct
		{
			MCExpression *prompt;
			MCExpression *answer;
			bool hint : 1;
		} password;
		struct
		{
			MCExpression *prompt;
			MCExpression *initial;
			MCExpression *filter;
			MCExpression **types;
			uint4 type_count;
		} file;
	};

public:
	MCAsk(void)
	{
		title = NULL;
		file . prompt = file . initial = file . filter = NULL;
		file . types = NULL;
		file . type_count = 0;
		mode = AT_UNDEFINED;
		sheet = False;
	}
	virtual ~MCAsk(void);

	virtual Parse_stat parse(MCScriptPoint &);
	virtual void exec_ctxt(MCExecContext &);

private:
	Parse_errors parse_question(MCScriptPoint& sp);
	Parse_errors parse_password(MCScriptPoint& sp);
	Parse_errors parse_file(MCScriptPoint& sp);
};

#endif
