#ifndef __IMAGE_CACHE_H__
#define __IMAGE_CACHE_H__

#include "idlib/hashing/MD4.h"
#include <string.h>

static bool R_HashedTextureCacheName( const char *root, const char *extension,
		const char *program, const int *options, unsigned int optionCount,
		char *path, size_t pathSize ) {
	unsigned char key[384];
	size_t length = strlen(program) + 1;
	if ( length + optionCount * 4 > sizeof(key) ) return false;
	memcpy(key, program, length);
	for ( unsigned int i = 0; i < optionCount; ++i ) {
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
	const size_t rootLength = strlen(root);
	const size_t extensionLength = strlen(extension);
	const size_t required = rootLength + 6 + 29 + extensionLength + 1;
	if ( pathSize < required ) return false;
	memcpy(path, root, rootLength);
	char *out = path + rootLength;
	for ( int i = 0; i < 3; ++i ) { *out++ = hex[i]; *out++ = '/'; }
	memcpy(out, hex + 3, 29);
	memcpy(out + 29, extension, extensionLength + 1);
	return true;
}

// Use 16 children per directory and hash the image program with pixel-affecting options.
static bool R_TextureCacheName( const char *program, int width, int height,
	int depth, int repeat, int level, bool colorMips, char *path, size_t pathSize ) {
	const int options[] = { width, height, depth, repeat, level, (int)colorMips };
	return R_HashedTextureCacheName("texturecache/v1/", ".etc", program, options,
		sizeof(options) / sizeof(options[0]), path, pathSize);
}

static bool R_PreparedTextureCacheName( const char *program, const int *options,
		unsigned int optionCount, char *path, size_t pathSize ) {
	return R_HashedTextureCacheName("texturecache/v2/", ".tex", program, options,
		optionCount, path, pathSize);
}

// Legacy .etc stores one format byte followed by pixels and relies on caller dimensions.
static int R_TextureCacheSize( int width, int height, int format ) {
	if ( width <= 0 || height <= 0 || width > 16384 || height > 16384 ) return -1;
	if ( format == 0 ) return (((width + 3) & ~3) * ((height + 3) & ~3)) / 2 + 1;
	if ( format == 1 ) return width * height * 2 + 1;
	return -1;
}

// Reject wrong lengths before allocating/reading. Never upload partial data.
static byte *R_ReadTextureCache( idFile *file, int width, int height ) {
	int length = file->Length();
	int etcSize = R_TextureCacheSize(width, height, 0);
	int rgbaSize = R_TextureCacheSize(width, height, 1);
	if ( etcSize < 0 || (length != etcSize && length != rgbaSize) ) return NULL;
	byte *data = (byte *)malloc(length);
	if ( !data ) return NULL;
	if ( file->Read(data, length) != length || R_TextureCacheSize(width, height, data[0]) != length ) {
		free(data);
		return NULL;
	}
	return data;
}

enum { PREPARED_TEXTURE_HEADER_SIZE = 20 };

static void R_StoreCacheLE32( byte *out, unsigned int value ) {
	out[0] = (byte)value;
	out[1] = (byte)(value >> 8);
	out[2] = (byte)(value >> 16);
	out[3] = (byte)(value >> 24);
}

static unsigned int R_LoadCacheLE32( const byte *in ) {
	return (unsigned int)in[0] | ((unsigned int)in[1] << 8) |
		((unsigned int)in[2] << 16) | ((unsigned int)in[3] << 24);
}

static void R_BuildPreparedTextureHeader( byte *header, int format, int depth,
		int width, int height, int payloadSize ) {
	memcpy(header, "D3TC", 4);
	header[4] = 2;
	header[5] = (byte)format;
	header[6] = (byte)depth;
	header[7] = 0;
	R_StoreCacheLE32(header + 8, (unsigned int)width);
	R_StoreCacheLE32(header + 12, (unsigned int)height);
	R_StoreCacheLE32(header + 16, (unsigned int)payloadSize);
}

// Validate an owned v2 entry and move its payload in place to avoid another allocation.
static byte *R_ConsumePreparedTextureCache( byte *contents, int length,
		int *format, int *depth, int *width, int *height, int *payloadSize ) {
	if ( !contents || length < PREPARED_TEXTURE_HEADER_SIZE ) {
		free(contents);
		return NULL;
	}
	if ( memcmp(contents, "D3TC", 4) || contents[4] != 2 || contents[5] > 1 ) {
		free(contents);
		return NULL;
	}
	const unsigned int w = R_LoadCacheLE32(contents + 8);
	const unsigned int h = R_LoadCacheLE32(contents + 12);
	const unsigned int size = R_LoadCacheLE32(contents + 16);
	if ( w == 0 || h == 0 || w > 16384 || h > 16384 ) {
		free(contents);
		return NULL;
	}
	const int expected = R_TextureCacheSize((int)w, (int)h, contents[5]) - 1;
	if ( expected < 0 || size != (unsigned int)expected ||
			length != PREPARED_TEXTURE_HEADER_SIZE + expected ) {
		free(contents);
		return NULL;
	}
	*format = contents[5];
	*depth = contents[6];
	*width = (int)w;
	*height = (int)h;
	*payloadSize = expected;
	memmove(contents, contents + PREPARED_TEXTURE_HEADER_SIZE, expected);
	return contents;
}

// The v2 cache includes dimensions so it can be loaded before the source image.
static byte *R_ReadPreparedTextureCache( idFile *file, int *format, int *depth,
		int *width, int *height, int *payloadSize ) {
	const int length = file->Length();
	if ( length < PREPARED_TEXTURE_HEADER_SIZE ) return NULL;
	byte *contents = (byte *)malloc(length);
	if ( !contents || file->Read(contents, length) != length ) {
		free(contents);
		return NULL;
	}
	return R_ConsumePreparedTextureCache(contents, length, format, depth,
		width, height, payloadSize);
}

#endif
