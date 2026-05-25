#include "regex.h"

////////////////////////////////////////////////////////////////////////////////

// JS-2013-07-01: [[ EnhancedFilter ]] Utility class and descendents handling the
//   regex and wildcard style pattern matchers.
class MCPatternMatcher
{
protected:
    MCStringRef m_pattern;
    MCStringRef m_string_source;
    MCArrayRef m_array_source;
    MCStringOptions m_options;
public:
    MCPatternMatcher(MCStringRef p_pattern, MCStringRef p_string, MCStringOptions p_options);
    MCPatternMatcher(MCStringRef p_pattern, MCArrayRef p_array, MCStringOptions p_options);
    virtual ~MCPatternMatcher();
    virtual bool compile(MCStringRef& r_error) = 0;
    virtual bool match(MCExecContext& ctxt, MCRange p_range) = 0;
    virtual bool match(MCExecContext& ctxt, MCNameRef p_key, bool p_match_key) = 0;
    MCStringRef getstringsource()
    {
        return m_string_source;
    }
};

class MCRegexMatcher : public MCPatternMatcher
{
protected:
    regexp *m_compiled;
public:
    MCRegexMatcher(MCStringRef p_pattern, MCStringRef p_string, MCStringOptions p_options);
    MCRegexMatcher(MCStringRef p_pattern, MCArrayRef p_array, MCStringOptions p_options);
    virtual ~MCRegexMatcher();
    virtual bool compile(MCStringRef& r_error);
    virtual bool match(MCExecContext& ctxt, MCRange p_range);
    virtual bool match(MCExecContext& ctxt, MCNameRef p_key, bool p_match_key);
};

class MCWildcardMatcher : public MCPatternMatcher
{
    bool m_native;
public:
    MCWildcardMatcher(MCStringRef p_pattern, MCStringRef p_string, MCStringOptions p_options);
    MCWildcardMatcher(MCStringRef p_pattern, MCArrayRef p_array, MCStringOptions p_options);
    virtual ~MCWildcardMatcher();
    virtual bool compile(MCStringRef& r_error);
    virtual bool match(MCExecContext& ctxt, MCRange p_range);
    virtual bool match(MCExecContext& ctxt, MCNameRef p_key, bool p_match_key);
protected:
    static bool match(const char *s, const char *p, Boolean cs);
};

class MCExpressionMatcher : public MCPatternMatcher
{
    MCExpression * m_expression;
public:
    MCExpressionMatcher(MCExpression* p_expression, MCStringRef p_string, MCStringOptions p_options);
    MCExpressionMatcher(MCExpression* p_expression, MCArrayRef p_array, MCStringOptions p_options);
    virtual ~MCExpressionMatcher();
    virtual bool compile(MCStringRef& r_error);
    virtual bool match(MCExecContext& ctxt, MCRange p_range);
    virtual bool match(MCExecContext& ctxt, MCNameRef p_key, bool p_match_key);
protected:
    static bool match(const char *s, const char *p, Boolean cs);
};
