
extern uint1 MCS_langidtocharset(uint2 langid);
extern uint2 MCS_charsettolangid(uint1 charset);

extern void MCS_multibytetounicode(const char *s, uint4 len, char *d, uint4 destbufferl, uint4 &destlen, uint1 charset);
extern void MCS_unicodetomultibyte(const char *s, uint4 len, char *d, uint4 destbufferl, uint4 &destlen, uint1 charset);
