#if !defined(_SERVER)
static bool s_ssl_loaded = false;

extern "C" int initialise_weak_link_crypto(void);
extern "C" int initialise_weak_link_ssl(void);
bool load_ssl_library()
{
	if (s_ssl_loaded)
		return true;
    
	s_ssl_loaded = initialise_weak_link_crypto() && initialise_weak_link_ssl();
    
	return s_ssl_loaded;
}
#elif defined(_SERVER)
bool load_ssl_library()
{
	return true;
}
#endif
