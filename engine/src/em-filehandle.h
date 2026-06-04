#ifndef __MC_EMSCRIPTEN_FILE_HANDLE_H__
#define __MC_EMSCRIPTEN_FILE_HANDLE_H__

#include <foundation.h>

#include "globdefs.h"
#include "sysdefs.h"
#include "system.h"

struct MCEmscriptenFileHandle : public MCSystemFileHandle
{
public:
	/* Create a new file handle from the specified file descriptor,
	 * taking ownership of it.*/
	MCEmscriptenFileHandle(int p_fd);

	virtual ~MCEmscriptenFileHandle();

	virtual void Close(void);

	virtual bool PutBack(char p_char);
	virtual bool IsExhausted(void);

	virtual bool Read(void *x_buffer, uint32_t p_length, uint32_t & r_read);
	virtual bool Write(const void *p_buffer, uint32_t p_length);

	virtual bool Seek(int64_t p_offset, int p_whence);
	virtual int64_t Tell(void);

	virtual bool Truncate(void);
	virtual bool Sync(void);
	virtual bool Flush(void);

	virtual void *GetFilePointer(void);
	virtual uint64_t GetFileSize(void);

	virtual bool TakeBuffer(void *& r_buffer, size_t & r_length);

protected:
	int m_fd;
	bool m_eof;
};

#endif /* !__MC_EMSCRIPTEN_FILE_HANDLE_H__ */
