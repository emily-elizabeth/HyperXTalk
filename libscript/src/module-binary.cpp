#include <foundation.h>
#include <foundation-auto.h>
#include <foundation-chunk.h>

extern "C" MC_DLLEXPORT_DEF void MCBinaryEvalConcatenateBytes(MCDataRef p_left, MCDataRef p_right, MCDataRef& r_output)
{
    MCAutoDataRef t_data;
    if (!MCDataMutableCopy(p_left, &t_data))
        return;
    
    if (!MCDataAppend(*t_data, p_right))
        return;
    
    if (!MCDataCopy(*t_data, r_output))
        return;
}

extern "C" MC_DLLEXPORT_DEF void MCBinaryExecPutBytesBefore(MCDataRef p_source, MCDataRef& x_target)
{
    MCAutoDataRef t_data;
    MCBinaryEvalConcatenateBytes(p_source, x_target == (MCDataRef)kMCNull ? kMCEmptyData : x_target, &t_data);
    
    if (MCErrorIsPending())
        return;
    
    MCValueAssign(x_target, *t_data);
}

extern "C" MC_DLLEXPORT_DEF void MCBinaryExecPutBytesAfter(MCDataRef p_source, MCDataRef& x_target)
{
    MCAutoDataRef t_data;
    MCBinaryEvalConcatenateBytes(x_target == (MCDataRef)kMCNull ? kMCEmptyData : x_target, p_source, &t_data);
    
    if (MCErrorIsPending())
        return;
    
    MCValueAssign(x_target, *t_data);
}

extern "C" MC_DLLEXPORT_DEF void MCBinaryEvalIsEqualTo(MCDataRef p_left, MCDataRef p_right, bool& r_result)
{
    r_result = MCDataIsEqualTo(p_left, p_right);
}

extern "C" MC_DLLEXPORT_DEF void MCBinaryEvalIsNotEqualTo(MCDataRef p_left, MCDataRef p_right, bool& r_result)
{
    r_result = !MCDataIsEqualTo(p_left, p_right);
}

extern "C" MC_DLLEXPORT_DEF void MCBinaryEvalIsLessThan(MCDataRef p_left, MCDataRef p_right, bool& r_result)
{
    r_result = MCDataCompareTo(p_left, p_right) < 0;
}

extern "C" MC_DLLEXPORT_DEF void MCBinaryEvalIsGreaterThan(MCDataRef p_left, MCDataRef p_right, bool& r_result)
{
    r_result = MCDataCompareTo(p_left, p_right) > 0;
}

extern "C" MC_DLLEXPORT_DEF void MCDataEvalEmpty(MCDataRef& r_output)
{
    r_output = MCValueRetain(kMCEmptyData);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" bool com_livecode_binary_Initialize(void)
{
    return true;
}

extern "C" void com_livecode_binary_Finalize(void)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
