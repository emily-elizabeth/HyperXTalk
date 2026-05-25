#ifndef __MC_BSDIFF__
#define __MC_BSDIFF__

struct MCBsDiffInputStream
{
	virtual bool Measure(uint32_t& r_size) = 0;
	virtual bool ReadBytes(void *buffer, uint32_t count) = 0;
	virtual bool ReadInt32(int32_t& r_value) = 0;
};

struct MCBsDiffOutputStream
{
	virtual bool Rewind(void) = 0;
	virtual bool WriteBytes(const void *buffer, uint32_t count) = 0;
	virtual bool WriteInt32(int32_t value) = 0;
};

bool MCBsDiffBuild(MCBsDiffInputStream *old_stream, MCBsDiffInputStream *new_stream, MCBsDiffOutputStream *patch_stream);
bool MCBsDiffApply(MCBsDiffInputStream *patch_stream, MCBsDiffInputStream *input_stream, MCBsDiffOutputStream *output_stream);

#endif
