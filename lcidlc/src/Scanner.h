#ifndef __SCANNER__
#define __SCANNER__

#ifndef __VALUE__
#include "Value.h"
#endif

#ifndef __POSITION__
#include "Position.h"
#endif

////////////////////////////////////////////////////////////////////////////////

enum TokenType
{
	kTokenTypeNone,
	kTokenTypeEnd,
	kTokenTypeIdentifier,
	kTokenTypeNumber,
	kTokenTypeString,
	kTokenTypeComma,
	
	kTokenTypeError,
	kTokenTypeInvalidCharError,
	kTokenTypeUnterminatedStringError,
	kTokenTypeNewlineInStringError,
};

struct Token
{
	TokenType type;
	Position start, finish;
	ValueRef value;
};

////////////////////////////////////////////////////////////////////////////////

typedef struct Scanner *ScannerRef;

bool ScannerCreate(const char *p_filename, ScannerRef& r_scanner);
void ScannerDestroy(ScannerRef scanner);

bool ScannerAdvance(ScannerRef scanner);
bool ScannerRetreat(ScannerRef scanner);

void ScannerMark(ScannerRef scanner);

bool ScannerRetrieve(ScannerRef scanner, const Token*& r_token); 

////////////////////////////////////////////////////////////////////////////////

#endif
