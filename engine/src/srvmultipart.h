#ifndef __MC_SERVER_MULTIPART__
#define __MC_SERVER_MULTIPART__

#include "filedefs.h"

typedef enum
{
	kMCMultiPartFormData,
	kMCMultiPartFile,
} MCMultiPartType;


typedef enum
{
	kMCFileStatusOK,
	kMCFileStatusStopped,
	kMCFileStatusFailed,
	kMCFileStatusNoUploadFolder,
	kMCFileStatusIOError,
} MCMultiPartFileStatus;

typedef struct
{
	char *name;
	char *value;
	uint32_t param_count;
	char **param_name;
	char **param_value;
} MCMultiPartHeader;


typedef bool (*MCMultiPartHeaderCallback)(void *p_context, MCMultiPartHeader *p_header);
typedef bool (*MCMultiPartBodyCallback)(void *p_context, const char *p_data, uint32_t p_data_length, bool p_finished, bool p_truncated);

//read multipart message from stream, pass headers & data through callbacks

bool MCMultiPartReadMessageFromStream(IO_handle p_stream, MCStringRef p_boundary, uint32_t &r_bytes_read,
									  MCMultiPartHeaderCallback p_header_callback, MCMultiPartBodyCallback p_body_callback, void *p_context);

bool MCMultiPartCreateTempFile(MCStringRef p_temp_folder, IO_handle &r_file_handle, MCStringRef &r_temp_name);
void MCMultiPartRemoveTempFiles();


bool MCMultiPartGetErrorMessage(MCMultiPartFileStatus p_status, MCStringRef &r_message);


#endif // __MC_SERVER_MULTIPART__
