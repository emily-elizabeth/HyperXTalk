/*
	X11Audio : Provides an abstraction between the aclip object and a Linux based audio system.
	This uses EsounD (which comes installed as part of GNOME). However, this interface will be
    simple enough to allow us to plug in any sound engine (OSS, ALSA).
*/

#ifndef X11AUDIO_H
#define X11AUDIO_H



class X11Audio
{
public:
	X11Audio(void);
	~X11Audio(void);
	bool init(char * p_hostname, uint2 p_channels, uint2 p_width );
	int play(int1 * p_sample, uint4 p_samplesize, uint p_rate); 
	void close(void) ;
	int getloudness(void) { return 0; } ;
	void setloudness(int t_loudness) {} ;
	bool geterror(void) { return m_error ; } ; 
	int getfd (void) { return m_esd_connection; } ;
		
	
private:
	bool m_playing ;
	uint4 m_flags ;
	int	 m_esd_connection ;
	char * m_host ;
	bool m_error ;
	bool isinit(void) { return ( m_esd_connection > -1 ) ; } ;
} ;




#endif 
