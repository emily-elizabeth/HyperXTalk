#include "gtest/gtest.h"

#include "foundation.h"
#include "foundation-system.h"

//
// The libfoundation testing environment
//
class LibfoundationEnvironment : public ::testing::Environment {
public:
	virtual ~LibfoundationEnvironment() {}

	virtual void SetUp() {
		ASSERT_TRUE(MCInitialize());
        ASSERT_TRUE(MCSInitialize());
	}

	virtual void TearDown() {
        MCSFinalize();
		MCFinalize();
	}
};

// Register the environment
::testing::Environment* const libfoundation_env =
	::testing::AddGlobalTestEnvironment(new LibfoundationEnvironment);
