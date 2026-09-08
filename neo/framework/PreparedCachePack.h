#ifndef __PREPARED_CACHE_PACK_H__
#define __PREPARED_CACHE_PACK_H__

enum preparedCachePackType_t {
	PREPARED_CACHE_PACK_TEXTURE,
	PREPARED_CACHE_PACK_SOUND,
	PREPARED_CACHE_PACK_COUNT
};

bool PreparedCachePack_Read( preparedCachePackType_t type, const char *cacheName,
	unsigned char **data, int *length );
bool PreparedCachePack_Write( preparedCachePackType_t type, const char *cacheName,
	const void *header, int headerLength, const void *payload, int payloadLength );
void PreparedCachePack_Shutdown();

#endif
