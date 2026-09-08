#ifndef __SOUND_CACHE_H__
#define __SOUND_CACHE_H__

#include "idlib/hashing/MD4.h"
#include <string.h>

struct preparedSoundInfo_t {
	unsigned int formatTag;
	unsigned int channels;
	unsigned int samplesPerSec;
	unsigned int avgBytesPerSec;
	unsigned int blockAlign;
	unsigned int bitsPerSample;
	unsigned int extraSize;
	unsigned int objectSize;
	unsigned int objectMemSize;
};

enum {
	PREPARED_SOUND_HEADER_SIZE = 44,
	PREPARED_SOUND_MAX_DATA_SIZE = 64 * 1024 * 1024
};

static void S_StoreCacheLE32( unsigned char *out, unsigned int value ) {
	out[0] = (unsigned char)value;
	out[1] = (unsigned char)(value >> 8);
	out[2] = (unsigned char)(value >> 16);
	out[3] = (unsigned char)(value >> 24);
}

static unsigned int S_LoadCacheLE32( const unsigned char *in ) {
	return (unsigned int)in[0] | ((unsigned int)in[1] << 8) |
		((unsigned int)in[2] << 16) | ((unsigned int)in[3] << 24);
}

static bool S_PreparedSoundCacheName( const char *name, int force22kHz,
		int realTimeDecoding, int decompressionLimit, char *path, size_t pathSize ) {
	unsigned char key[384];
	size_t length = strlen(name) + 1;
	if ( length + 12 > sizeof(key) ) return false;
	memcpy(key, name, length);
	const int options[] = { force22kHz, realTimeDecoding, decompressionLimit };
	for ( unsigned int i = 0; i < sizeof(options) / sizeof(options[0]); ++i ) {
		for ( int shift = 0; shift < 32; shift += 8 ) {
			key[length++] = (unsigned int)options[i] >> shift;
		}
	}

	unsigned char digest[16];
	MD4_BlockDigest(key, length, digest);
	const char *digits = "0123456789abcdef";
	char hex[33];
	for ( int i = 0; i < 16; ++i ) {
		hex[i * 2] = digits[digest[i] >> 4];
		hex[i * 2 + 1] = digits[digest[i] & 15];
	}
	hex[32] = 0;

	const char *root = "soundcache/v1/";
	const char *extension = ".snd";
	const size_t required = strlen(root) + 6 + 29 + strlen(extension) + 1;
	if ( pathSize < required ) return false;
	char *out = path;
	memcpy(out, root, strlen(root));
	out += strlen(root);
	for ( int i = 0; i < 3; ++i ) {
		*out++ = hex[i];
		*out++ = '/';
	}
	memcpy(out, hex + 3, 29);
	memcpy(out + 29, extension, strlen(extension) + 1);
	return true;
}

static void S_BuildPreparedSoundHeader( unsigned char *header,
		const preparedSoundInfo_t &info ) {
	memcpy(header, "D3SC", 4);
	S_StoreCacheLE32(header + 4, 1);
	S_StoreCacheLE32(header + 8, info.formatTag);
	S_StoreCacheLE32(header + 12, info.channels);
	S_StoreCacheLE32(header + 16, info.samplesPerSec);
	S_StoreCacheLE32(header + 20, info.avgBytesPerSec);
	S_StoreCacheLE32(header + 24, info.blockAlign);
	S_StoreCacheLE32(header + 28, info.bitsPerSample);
	S_StoreCacheLE32(header + 32, info.extraSize);
	S_StoreCacheLE32(header + 36, info.objectSize);
	S_StoreCacheLE32(header + 40, info.objectMemSize);
}

static bool S_ParsePreparedSoundHeader( const unsigned char *header,
		unsigned int fileSize, preparedSoundInfo_t *info ) {
	if ( !header || !info || fileSize < PREPARED_SOUND_HEADER_SIZE ||
			memcmp(header, "D3SC", 4) || S_LoadCacheLE32(header + 4) != 1 ) {
		return false;
	}
	info->formatTag = S_LoadCacheLE32(header + 8);
	info->channels = S_LoadCacheLE32(header + 12);
	info->samplesPerSec = S_LoadCacheLE32(header + 16);
	info->avgBytesPerSec = S_LoadCacheLE32(header + 20);
	info->blockAlign = S_LoadCacheLE32(header + 24);
	info->bitsPerSample = S_LoadCacheLE32(header + 28);
	info->extraSize = S_LoadCacheLE32(header + 32);
	info->objectSize = S_LoadCacheLE32(header + 36);
	info->objectMemSize = S_LoadCacheLE32(header + 40);

	if ( (info->formatTag != 1 && info->formatTag != 2) ||
			(info->channels != 1 && info->channels != 2) ||
			(info->samplesPerSec != 11025 && info->samplesPerSec != 22050 &&
			 info->samplesPerSec != 44100) || info->bitsPerSample != 16 ||
			info->extraSize > 0xffffU ||
			info->objectSize == 0 || info->objectSize > 0x3fffffffU ||
			info->objectMemSize == 0 ||
			info->objectMemSize > PREPARED_SOUND_MAX_DATA_SIZE ||
			fileSize != PREPARED_SOUND_HEADER_SIZE + info->objectMemSize ) {
		return false;
	}
	if ( info->formatTag == 1 &&
			info->objectMemSize != info->objectSize * sizeof(short) ) {
		return false;
	}
	return true;
}

#endif
