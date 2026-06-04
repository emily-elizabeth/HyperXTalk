//
// create new script objects
//
class MCStatement;
class MCExpression;

extern MCStatement *MCN_new_statement(int2 which);
extern MCExpression *MCN_new_function(int2 which);
extern MCExpression *MCN_new_operator(int2 which);
