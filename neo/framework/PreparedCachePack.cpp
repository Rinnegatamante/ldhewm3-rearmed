#include "sys/platform.h"
#include "framework/PreparedCachePack.h"

#if defined(VITA) || defined(__vita__)
#include "framework/FileSystem.h"
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <pthread.h>
#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace {

enum {
	PACK_SHARDS = 16,
	PACK_RECORD_SIZE = 32,
	PACK_MAX_ENTRY_SIZE = 64 * 1024 * 1024
};

struct packEntry_t {
	unsigned char key[16];
	uint32_t offset;
	uint32_t length;
};

struct packShard_t {
	SceUID dataFd;
	SceOff dataSize;
	SceOff indexSize;
	packEntry_t *entries;
	int numEntries;
	int maxEntries;
	bool readAttempted;
	bool ready;
};

static packShard_t packStates[PREPARED_CACHE_PACK_COUNT][PACK_SHARDS];
static bool packStatesInitialized = false;
static pthread_mutex_t packMutex = PTHREAD_MUTEX_INITIALIZER;

static const char *PackRoot( preparedCachePackType_t type ) {
	return type == PREPARED_CACHE_PACK_TEXTURE ?
		"texturecache/packed-v1" : "soundcache/packed-v1";
}

static const char *CachePrefix( preparedCachePackType_t type ) {
	return type == PREPARED_CACHE_PACK_TEXTURE ?
		"texturecache/v2/" : "soundcache/v1/";
}

static void InitPackStates() {
	if ( packStatesInitialized ) {
		return;
	}
	for ( int type = 0; type < PREPARED_CACHE_PACK_COUNT; ++type ) {
		for ( int shard = 0; shard < PACK_SHARDS; ++shard ) {
			packStates[type][shard].dataFd = -1;
		}
	}
	packStatesInitialized = true;
}

static int HexDigit( char c ) {
	if ( c >= '0' && c <= '9' ) return c - '0';
	if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
	if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
	return -1;
}

static bool CacheKey( preparedCachePackType_t type, const char *cacheName,
		unsigned char key[16] ) {
	if ( !cacheName ) return false;
	const char *prefix = CachePrefix( type );
	const size_t prefixLength = strlen( prefix );
	if ( strlen(cacheName) < prefixLength ||
			memcmp( cacheName, prefix, prefixLength ) ) return false;
	const char *p = cacheName + prefixLength;
	int nibble = 0;
	memset( key, 0, 16 );
	while ( *p && *p != '.' && nibble < 32 ) {
		if ( *p != '/' && *p != '\\' ) {
			const int value = HexDigit( *p );
			if ( value < 0 ) return false;
			if ( nibble & 1 ) key[nibble >> 1] |= (unsigned char)value;
			else key[nibble >> 1] = (unsigned char)(value << 4);
			++nibble;
		}
		++p;
	}
	return nibble == 32 && *p == '.';
}

static uint32_t LoadLE32( const unsigned char *in ) {
	return (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
		((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
}

static void StoreLE32( unsigned char *out, uint32_t value ) {
	out[0] = (unsigned char)value;
	out[1] = (unsigned char)(value >> 8);
	out[2] = (unsigned char)(value >> 16);
	out[3] = (unsigned char)(value >> 24);
}

static bool PReadExact( SceUID fd, void *buffer, int length, SceOff offset ) {
	unsigned char *out = (unsigned char *)buffer;
	int done = 0;
	while ( done < length ) {
		const int result = sceIoPread( fd, out + done, length - done, offset + done );
		if ( result <= 0 ) return false;
		done += result;
	}
	return true;
}

static bool PWriteExact( SceUID fd, const void *buffer, int length, SceOff offset ) {
	const unsigned char *in = (const unsigned char *)buffer;
	int done = 0;
	while ( done < length ) {
		const int result = sceIoPwrite( fd, in + done, length - done, offset + done );
		if ( result <= 0 ) return false;
		done += result;
	}
	return true;
}

static bool EnsureEntryCapacity( packShard_t &state ) {
	if ( state.numEntries == state.maxEntries ) {
		const int newMax = state.maxEntries ? state.maxEntries * 2 : 128;
		packEntry_t *newEntries = (packEntry_t *)realloc( state.entries,
			newMax * sizeof(packEntry_t) );
		if ( !newEntries ) return false;
		state.entries = newEntries;
		state.maxEntries = newMax;
	}
	return true;
}

static bool AppendEntry( packShard_t &state, const packEntry_t &entry ) {
	if ( !EnsureEntryCapacity( state ) ) return false;
	state.entries[state.numEntries++] = entry;
	return true;
}

static void CreateDirectoryTree( idStr path ) {
	path.BackSlashesToSlashes();
	for ( int i = 1; i < path.Length(); ++i ) {
		if ( path[i] == '/' ) {
			const char saved = path[i];
			path[i] = 0;
			sceIoMkdir( path.c_str(), 0777 );
			path[i] = saved;
		}
	}
	sceIoMkdir( path.c_str(), 0777 );
}

static bool LoadIndex( packShard_t &state, SceUID indexFd ) {
	if ( state.indexSize <= 0 ) return true;
	if ( state.indexSize > 32 * 1024 * 1024 ) return false;
	unsigned char *bytes = (unsigned char *)malloc( (size_t)state.indexSize );
	if ( !bytes ) return false;
	if ( !PReadExact( indexFd, bytes, (int)state.indexSize, 0 ) ) {
		free( bytes );
		return false;
	}
	const int records = (int)(state.indexSize / PACK_RECORD_SIZE);
	for ( int i = 0; i < records; ++i ) {
		const unsigned char *record = bytes + i * PACK_RECORD_SIZE;
		if ( memcmp( record, "D3IX", 4 ) || LoadLE32( record + 28 ) != 1 ) continue;
		packEntry_t entry;
		memcpy( entry.key, record + 4, sizeof(entry.key) );
		entry.offset = LoadLE32( record + 20 );
		entry.length = LoadLE32( record + 24 );
		if ( entry.length == 0 || entry.length > PACK_MAX_ENTRY_SIZE ||
			(uint64_t)entry.offset + entry.length > (uint64_t)state.dataSize ) continue;
		if ( !AppendEntry( state, entry ) ) {
			free( bytes );
			return false;
		}
	}
	// Ignore and overwrite a torn trailing record after an interrupted write.
	state.indexSize = (SceOff)records * PACK_RECORD_SIZE;
	free( bytes );
	return true;
}

static void ShardPaths( preparedCachePackType_t type, int shard,
		idStr &directory, idStr &dataPath, idStr &indexPath ) {
	idStr relativeDir = va( "%s/%x", PackRoot( type ), shard );
	directory = fileSystem->RelativePathToOSPath( relativeDir, "fs_savepath" );
	dataPath = directory + "/data.bin";
	indexPath = directory + "/index.bin";
}

static bool OpenShard( preparedCachePackType_t type, int shard, bool create ) {
	packShard_t &state = packStates[type][shard];
	if ( state.ready && state.dataFd >= 0 ) return true;
	if ( state.readAttempted && !create ) return false;
	state.readAttempted = true;

	idStr osDir, dataPath, indexPath;
	ShardPaths( type, shard, osDir, dataPath, indexPath );
	if ( create ) CreateDirectoryTree( osDir );
	const int flags = SCE_O_RDWR | ( create ? SCE_O_CREAT : 0 );
	state.dataFd = sceIoOpen( dataPath.c_str(), flags, 0777 );
	const SceUID indexFd = sceIoOpen( indexPath.c_str(), flags, 0777 );
	if ( state.dataFd < 0 || indexFd < 0 ) {
		if ( state.dataFd >= 0 ) sceIoClose( state.dataFd );
		if ( indexFd >= 0 ) sceIoClose( indexFd );
		state.dataFd = -1;
		return false;
	}
	SceIoStat stat;
	if ( sceIoGetstatByFd( state.dataFd, &stat ) < 0 ) {
		sceIoClose( state.dataFd );
		sceIoClose( indexFd );
		state.dataFd = -1;
		return false;
	}
	state.dataSize = stat.st_size;
	if ( sceIoGetstatByFd( indexFd, &stat ) < 0 ) {
		sceIoClose( state.dataFd );
		sceIoClose( indexFd );
		state.dataFd = -1;
		return false;
	}
	state.indexSize = stat.st_size;
	const bool loaded = LoadIndex( state, indexFd );
	sceIoClose( indexFd );
	if ( !loaded ) {
		sceIoClose( state.dataFd );
		state.dataFd = -1;
		free( state.entries );
		state.entries = NULL;
		state.numEntries = state.maxEntries = 0;
		return false;
	}
	state.ready = true;
	return true;
}

}

bool PreparedCachePack_Read( preparedCachePackType_t type, const char *cacheName,
		unsigned char **data, int *length ) {
	if ( data ) *data = NULL;
	if ( length ) *length = 0;
	if ( type < 0 || type >= PREPARED_CACHE_PACK_COUNT || !data || !length ) return false;
	unsigned char key[16];
	if ( !CacheKey( type, cacheName, key ) ) return false;
	const int shard = key[0] >> 4;
	SceUID dataFd = -1;
	uint32_t entryOffset = 0;
	uint32_t entryLength = 0;
	pthread_mutex_lock( &packMutex );
	InitPackStates();
	if ( !OpenShard( type, shard, false ) ) {
		pthread_mutex_unlock( &packMutex );
		return false;
	}
	packShard_t &state = packStates[type][shard];
	for ( int i = state.numEntries - 1; i >= 0; --i ) {
		const packEntry_t &entry = state.entries[i];
		if ( memcmp( entry.key, key, sizeof(key) ) ) continue;
		dataFd = state.dataFd;
		entryOffset = entry.offset;
		entryLength = entry.length;
		pthread_mutex_unlock( &packMutex );
		// sceIo performs best with cache-line-aligned transfer buffers.
		unsigned char *contents = (unsigned char *)memalign( 64, entryLength );
		if ( !contents || !PReadExact(dataFd, contents, entryLength, entryOffset) ) {
			free( contents );
			return false;
		}
		*data = contents;
		*length = (int)entryLength;
		return true;
	}
	pthread_mutex_unlock( &packMutex );
	return false;
}

bool PreparedCachePack_Write( preparedCachePackType_t type, const char *cacheName,
		const void *header, int headerLength, const void *payload, int payloadLength ) {
	if ( type < 0 || type >= PREPARED_CACHE_PACK_COUNT || !header || headerLength <= 0 ||
		!payload || payloadLength <= 0 || headerLength > PACK_MAX_ENTRY_SIZE ||
		payloadLength > PACK_MAX_ENTRY_SIZE ||
		headerLength > PACK_MAX_ENTRY_SIZE - payloadLength ) return false;
	unsigned char key[16];
	if ( !CacheKey( type, cacheName, key ) ) return false;
	const int shard = key[0] >> 4;
	pthread_mutex_lock( &packMutex );
	InitPackStates();
	if ( !OpenShard( type, shard, true ) ) {
		pthread_mutex_unlock( &packMutex );
		return false;
	}
	packShard_t &state = packStates[type][shard];
	const int totalLength = headerLength + payloadLength;
	if ( state.dataSize < 0 || (uint64_t)state.dataSize + totalLength > 0xffffffffULL ) {
		pthread_mutex_unlock( &packMutex );
		return false;
	}
	packEntry_t entry;
	memcpy( entry.key, key, sizeof(key) );
	entry.offset = (uint32_t)state.dataSize;
	entry.length = (uint32_t)totalLength;
	// Reserve memory before publishing the disk index so failed writes can reuse the slot.
	if ( !EnsureEntryCapacity( state ) ) {
		pthread_mutex_unlock( &packMutex );
		return false;
	}
	idStr osDir, dataPath, indexPath;
	ShardPaths( type, shard, osDir, dataPath, indexPath );
	const SceUID indexFd = sceIoOpen( indexPath.c_str(), SCE_O_WRONLY, 0777 );
	if ( indexFd < 0 ) {
		pthread_mutex_unlock( &packMutex );
		return false;
	}
	if ( !PWriteExact( state.dataFd, header, headerLength, state.dataSize ) ||
		!PWriteExact( state.dataFd, payload, payloadLength, state.dataSize + headerLength ) ) {
		sceIoClose( indexFd );
		pthread_mutex_unlock( &packMutex );
		return false;
	}
	unsigned char record[PACK_RECORD_SIZE];
	memcpy( record, "D3IX", 4 );
	memcpy( record + 4, key, sizeof(key) );
	StoreLE32( record + 20, entry.offset );
	StoreLE32( record + 24, entry.length );
	StoreLE32( record + 28, 1 );
	if ( !PWriteExact( indexFd, record, sizeof(record), state.indexSize ) ) {
		sceIoClose( indexFd );
		pthread_mutex_unlock( &packMutex );
		return false;
	}
	sceIoClose( indexFd );
	state.dataSize += totalLength;
	state.indexSize += sizeof(record);
	state.entries[state.numEntries++] = entry;
	pthread_mutex_unlock( &packMutex );
	return true;
}

void PreparedCachePack_Shutdown() {
	pthread_mutex_lock( &packMutex );
	InitPackStates();
	for ( int type = 0; type < PREPARED_CACHE_PACK_COUNT; ++type ) {
		for ( int shard = 0; shard < PACK_SHARDS; ++shard ) {
			packShard_t &state = packStates[type][shard];
			if ( state.dataFd >= 0 ) sceIoClose( state.dataFd );
			free( state.entries );
			memset( &state, 0, sizeof(state) );
			state.dataFd = -1;
		}
	}
	pthread_mutex_unlock( &packMutex );
}

#else

bool PreparedCachePack_Read( preparedCachePackType_t, const char *, unsigned char **data, int *length ) {
	if ( data ) *data = NULL;
	if ( length ) *length = 0;
	return false;
}

bool PreparedCachePack_Write( preparedCachePackType_t, const char *, const void *, int,
	const void *, int ) {
	return false;
}

void PreparedCachePack_Shutdown() {}

#endif
