#ifndef __TEST__
#define __TEST__

//////////

struct MCLowLevelTest
{
    const char *name;
    const char *file;
    int line;
    void (*handler)(void);
};

int MCCountLowLevelTests(void);
void MCExecuteLowLevelTest(int index);

#if defined(_WINDOWS)
#define TEST_DEFINE_SECTION(Name) \
__declspec(allocate(".test")) volatile MCLowLevelTest *Name##_Ptr = &Name;
#elif defined(_LINUX) || defined(TARGET_SUBPLATFORM_ANDROID)
#define TEST_DEFINE_SECTION(Name) \
__attribute__((section(".test"))) volatile MCLowLevelTest *Name##_Ptr = &Name;
#elif defined(_MACOSX) || defined(TARGET_SUBPLATFORM_IPHONE)
#define TEST_DEFINE_SECTION(Name) \
__attribute__((section("__TEST,__test"))) volatile MCLowLevelTest *Name##_Ptr = &Name;
#endif

#define TEST_DEFINE(Name, Handler) \
static MCLowLevelTest kMCLowLevelTest_##Name = { #Name, __FILE__, __LINE__, Handler }; \
TEST_DEFINE_SECTION(kMCLowLevelTest_##Name)

//////////

struct MCHighLevelTest;

int MCCountHighLevelTests(void);
void MCExecuteHighLevelTest(int index);

void MCTestDoAbort(const char *message, const char *file, int line, bool p_is_high_level = false);
void MCTestDoAssertTrue(const char *message, bool value, const char *file, int line, bool p_is_high_level = false);
void MCTestDoAssertFalse(const char *message, bool value, const char *file, int line, bool p_is_high_level = false);

#define MCTestAbort(message) MCTestDoAbort(message, __FILE__, __LINE__)
#define MCTestAssertTrue(message, value) MCTestDoAssertTrue(message, value, __FILE__, __LINE__)
#define MCTestAssertFalse(message, value) MCTestDoAssertFalse(message, value, __FILE__, __LINE__)

//////////

#endif
