// Stub implementations of MCJava* functions for platforms that do not
// support Java (mac desktop, etc.).

#if !defined(TARGET_SUPPORTS_JAVA)

#include "foundation.h"
#include "foundation-private.h"

MC_DLLEXPORT bool MCJavaVMInitialize() { return false; }
MC_DLLEXPORT bool MCJavaCheckSignature(MCTypeInfoRef p_signature, MCStringRef p_params, MCStringRef p_return, int p_call_type) { return false; }
MC_DLLEXPORT void *MCJavaGetMethodId(MCNameRef p_class_name, MCStringRef p_method_name, MCStringRef p_arguments, MCStringRef p_return, int p_call_type) { return nullptr; }

bool __MCJavaInitialize(void) { return true; }
void __MCJavaFinalize(void) {}

#endif // !TARGET_SUPPORTS_JAVA

MC_DLLEXPORT MCTypeInfoRef MCJavaGetObjectTypeInfo() { return nil; }
MC_DLLEXPORT bool MCJavaCreateJavaObjectTypeInfo() { return false; }
MC_DLLEXPORT bool MCJavaObjectCreate(void *value, MCJavaObjectRef& r_obj) { return false; }
MC_DLLEXPORT void *MCJavaObjectGetObject(const MCJavaObjectRef p_obj) { return nullptr; }
MC_DLLEXPORT bool MCJavaGetJObjectClassName(MCJavaObjectRef p_object, MCStringRef &r_name) { return false; }
MC_DLLEXPORT bool MCJavaConvertJStringToStringRef(MCJavaObjectRef p_object, MCStringRef &r_string) { return false; }
MC_DLLEXPORT bool MCJavaConvertStringRefToJString(MCStringRef p_string, MCJavaObjectRef &r_object) { return false; }
MC_DLLEXPORT bool MCJavaConvertJByteArrayToDataRef(MCJavaObjectRef p_object, MCDataRef &r_data) { return false; }
MC_DLLEXPORT bool MCJavaConvertDataRefToJByteArray(MCDataRef p_data, MCJavaObjectRef &r_object) { return false; }
