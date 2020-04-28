#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef int8_t		s8;
typedef int16_t		s16;
typedef int32_t		s32;

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define	U16B(x)		(x)
#define	U32B(x)		(x)
#define	U64B(x)		(x)
#define	U16L(x)		__builtin_bswap16(x)
#define	U32L(x)		__builtin_bswap32(x)
#define	U64L(x)		__builtin_bswap64(x)
#else
#define	U16B(x)		__builtin_bswap16(x)
#define	U32B(x)		__builtin_bswap32(x)
#define	U64B(x)		__builtin_bswap64(x)
#define	U16L(x)		(x)
#define	U32L(x)		(x)
#define	U64L(x)		(x)
#endif

typedef struct {
	u32	channels;
	u32	sample_count;
	u32	sample_rate;
	u32	nibble_count;
	u16	loop_flag;
	u16	format;
	u32	loop_start;
	u32	loop_end;
	u16	coef[0];
} RS02;

typedef struct {
	u32	sample_count;
	u32	nibble_count;
	u32	sample_rate;
	u16	loop_flag;
	u16	format;
	u32	sa;
	u32	ea;
	u32	ca;
	u16	coef[16];
	u16	gain;
	u16	ps;
	u16	yn1;
	u16	yn2;
	u16	lps;
	u16	lyn1;
	u16	lyn2;
	u16	pad[11];
} DSP;

inline u32 offset_to_samples(u32 offset, u32 channels)
{
	u32 bytes = offset / channels;
	return bytes * 14 / 8;
}

int main(int argc, char** argv)
{
	if(argc < 3) {
		printf("Usage: %s in.dsp out-1.dsp ...\n", *argv);
		return 1;
	}

	FILE* in = fopen(argv[1], "rb");
	fseek(in, 0, SEEK_END);
	size_t len = ftell(in);
	fseek(in, 0, SEEK_SET);

	void* data = malloc(len);
	fread(data, len, 1, in);
	fclose(in);

	RS02* rs02 = (RS02*) data;
	if(rs02->loop_flag) {
		printf("%d channels, %u Hz, %u samples, looped [%u - %u]\n",
				U32B(rs02->channels), U32B(rs02->sample_rate),
				U32B(rs02->sample_count),
				offset_to_samples(U32B(rs02->loop_start), U32B(rs02->channels)),
				offset_to_samples(U32B(rs02->loop_end), U32B(rs02->channels)));
	} else {
		printf("%d channels, %u Hz, %u samples, not looped\n",
				U32B(rs02->channels), U32B(rs02->sample_rate),
				U32B(rs02->sample_count));
	}

	if(U32B(rs02->channels) + 2 != argc) {
		printf("Invalid argument count\n");
		return 1;
	}

	char* adpcm = (char*) data + 0x60;
	for(int i = 0; i < U32B(rs02->channels); i++) {
		DSP header;
		memset(&header, 0, sizeof(header));
		header.sample_count = rs02->sample_count;
		header.nibble_count = rs02->nibble_count;
		header.sample_rate = rs02->sample_rate;
		header.loop_flag = rs02->loop_flag;
		header.format = rs02->format;
		header.sa = U32B(U32B(rs02->loop_start) / U32B(rs02->channels) * 2);
		header.ea = U32B(U32B(rs02->loop_end) / U32B(rs02->channels) * 2);
		header.ca = 0;
		header.ps = U16B(adpcm[i]);
		header.yn1 = 0;
		header.yn2 = 0;
		if(header.loop_flag) {
			header.lps = U16B(adpcm[i + (U32B(header.sa) / 16 * 8) * U32B(rs02->channels)]);
		} else {
			header.lps = 0;
		}
		header.lyn1 = 0;
		header.lyn2 = 0;

		for(int n = 0; n < 16; n++) {
			header.coef[n] = rs02->coef[i * 16 + n];
		}

		FILE* out = fopen(argv[i + 2], "wb");
		fwrite(&header, 1, sizeof(header), out);
		u32 end_offset = U32B(rs02->nibble_count) / 2 * U32B(rs02->channels);
		for(u32 byte = i; byte < end_offset; byte += U32B(rs02->channels)) {
			fwrite(&adpcm[byte], 1, 1, out);
		}
		fclose(out);
	}
}
