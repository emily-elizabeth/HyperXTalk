#include <foundation.h>
#include <foundation-auto.h>

static bool MCProperListCombine(void *context, MCValueRef p_value)
{
    MCListRef t_list = *(MCListRef *)context;
    
    if (MCValueGetTypeCode(p_value) != kMCValueTypeCodeString)
    {
        MCErrorCreateAndThrow(kMCGenericErrorTypeInfo, "reason", MCSTR("cannot combine list with non-string elements"), nil);
        return false;
    }
    
    return MCListAppend(t_list, p_value);
}

extern "C" MC_DLLEXPORT_DEF MCProperListRef MCTypeConvertExecSplitStringByDelimiter(MCStringRef p_target, MCStringRef p_delimiter)
{
    MCAutoProperListRef t_list;
    if (!MCStringSplitByDelimiter(p_target, p_delimiter, kMCStringOptionCompareExact, &t_list))
        return nil;
    
    return MCValueRetain(*t_list);
}

extern "C" MC_DLLEXPORT_DEF MCStringRef MCTypeConvertExecCombineListWithDelimiter(MCProperListRef p_target, MCStringRef p_delimiter)
{
    MCListRef t_list;
    if (!MCListCreateMutable(p_delimiter, t_list))
        return nil;
    
    if (!MCProperListApply(p_target, MCProperListCombine, &t_list))
    {
        MCValueRelease(t_list);
        return nil;
    }
   
    MCAutoStringRef t_string;
    if (!MCListCopyAsStringAndRelease(t_list, &t_string))
    {
        MCValueRelease(t_list);
        return nil;
    }
    
    return MCValueRetain(*t_string);
}

////////////////////////////////////////////////////////////////

extern "C" bool com_livecode_typeconvert_Initialize (void)
{
	return true;
}

extern "C" void com_livecode_typeconvert_Finalize (void)
{
}

////////////////////////////////////////////////////////////////
