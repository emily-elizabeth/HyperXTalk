#include "gtest/gtest.h"
#include "MCTapListener.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

int main(int argc, char **argv) {

#ifdef __EMSCRIPTEN__
	// Mount and cd into a nodefs filesystem so that emscripten tests
	// have the same access to the filesystem as native tests.  In
	// particular, they can write to a log file.
	EM_ASM(
		FS.mkdir('root');
		FS.mount(NODEFS, { root: '/' }, 'root');
		FS.chdir('root/' + process.cwd());
	);
#endif

	testing::InitGoogleTest(&argc, argv);

	for (int i = 0; i < argc; i++) {
		char* arg = argv[i];
		if (arg[0] == '-' &&
			arg[1] == '-' &&
			arg[2] == 't' &&
			arg[3] == 'a' &&
			arg[4] == 'p' &&
			arg[5] == '=') {

			testing::TestEventListeners& listeners
				= testing::UnitTest::GetInstance()->listeners();
			listeners.Append(new MCTapListener(&arg[6]));
		}
	}

	return RUN_ALL_TESTS();
}
