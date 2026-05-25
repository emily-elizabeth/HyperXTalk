#ifndef SRVSESSION_H
#define SRVSESSION_H

// session
typedef struct
{
	char*		id;
	char*		ip;
	char*		filename;
	real64_t	expires;
	
	MCSystemFileHandle * filehandle;
	
	// session data
	uint32_t	data_length;
	char *		data;
} MCSession, *MCSessionRef;

bool MCSessionStart(MCStringRef p_session_id, MCSessionRef &r_session);

bool MCSessionCommit(MCSessionRef p_session);
void MCSessionDiscard(MCSessionRef p_session);
bool MCSessionExpire(MCStringRef p_id);

bool MCSessionCleanup(void);

#endif//SRVSESSION_H
