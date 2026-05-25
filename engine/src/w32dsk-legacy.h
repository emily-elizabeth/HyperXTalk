extern unsigned char MCS_langidtocharset(unsigned short langid);
extern unsigned short MCS_charsettolangid(unsigned char charset);

extern void MCS_unicodetomultibyte(const char *s, unsigned int len, char *d,
                            unsigned int destbufferlength, unsigned int &destlen,
                            unsigned char charset);
extern void MCS_multibytetounicode(const char *s, unsigned int len, char *d,
                            unsigned int destbufferlength, unsigned int &destlen,
                            unsigned char charset);
