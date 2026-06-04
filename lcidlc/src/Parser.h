#ifndef __PARSER__
#define __PARSER__

#ifndef __SCANNER__
#include "Scanner.h"
#endif

#ifndef __INTERFACE__
#include "Interface.h"
#endif

bool ParserRun(ScannerRef scanner, InterfaceRef& r_interface);

#endif
