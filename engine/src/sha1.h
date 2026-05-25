#ifndef __MC_SHA1__
#define __MC_SHA1__

struct sha1_state_t
{
	uint32_t state[5];
	uint32_t count[2];
	uint8_t buffer[64];
};

void sha1_init(sha1_state_t *pms);
void sha1_append(sha1_state_t *pms, const void *data, uint32_t nbytes);
void sha1_finish(sha1_state_t *pms, uint8_t digest[20]);

#endif
