/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "sys/platform.h"
#include "framework/PreparedCachePack.h"
#include "framework/FileSystem.h"

#include "sound/snd_local.h"
#include "sound/Sound_cache.h"

#define USE_SOUND_CACHE_ALLOCATOR

#ifdef USE_SOUND_CACHE_ALLOCATOR
static idDynamicBlockAlloc<byte, 1<<20, 1<<10>	soundCacheAllocator;
#else
static idDynamicAlloc<byte, 1<<20, 1<<10>		soundCacheAllocator;
#endif

/*
===================
idSoundCache::idSoundCache()
===================
*/
idSoundCache::idSoundCache() {
	soundCacheAllocator.Init();
	soundCacheAllocator.SetLockMemory( true );
	listCache.AssureSize( 1024, NULL );
	listCache.SetGranularity( 256 );
	insideLevelLoad = false;
}

/*
===================
idSoundCache::~idSoundCache()
===================
*/
idSoundCache::~idSoundCache() {
	listCache.DeleteContents( true );
	soundCacheAllocator.Shutdown();
}

/*
===================
idSoundCache::::GetObject

returns a single cached object pointer
===================
*/
const idSoundSample* idSoundCache::GetObject( const int index ) const {
	if (index<0 || index>listCache.Num()) {
		return NULL;
	}
	return listCache[index];
}

/*
===================
idSoundCache::FindSound

Adds a sound object to the cache and returns a handle for it.
===================
*/
idSoundSample *idSoundCache::FindSound( const idStr& filename, bool loadOnDemandOnly ) {
	idStr fname;

	fname = filename;
	fname.BackSlashesToSlashes();
	fname.ToLower();

	declManager->MediaPrint( "%s\n", fname.c_str() );

	// check to see if object is already in cache
	for( int i = 0; i < listCache.Num(); i++ ) {
		idSoundSample *def = listCache[i];
		if ( def && def->name == fname ) {
			def->levelLoadReferenced = true;
			if ( def->purged && !loadOnDemandOnly ) {
				def->Load();
			}
			return def;
		}
	}

	// create a new entry
	idSoundSample *def = new idSoundSample;

	int shandle = listCache.FindNull();
	if ( shandle != -1 ) {
		listCache[shandle] = def;
	} else {
		shandle = listCache.Append( def );
	}

	def->name = fname;
	def->levelLoadReferenced = true;
	def->onDemand = loadOnDemandOnly;
	def->purged = true;

	if ( !loadOnDemandOnly ) {
		// this may make it a default sound if it can't be loaded
		def->Load();
	}

	return def;
}

/*
===================
idSoundCache::ReloadSounds

Completely nukes the current cache
===================
*/
void idSoundCache::ReloadSounds( bool force ) {
	int i;

	for( i = 0; i < listCache.Num(); i++ ) {
		idSoundSample *def = listCache[i];
		if ( def ) {
			def->Reload( force );
		}
	}
}

/*
====================
BeginLevelLoad

Mark all file based images as currently unused,
but don't free anything.  Calls to ImageFromFile() will
either mark the image as used, or create a new image without
loading the actual data.
====================
*/
void idSoundCache::BeginLevelLoad() {
	insideLevelLoad = true;

	for ( int i = 0 ; i < listCache.Num() ; i++ ) {
		idSoundSample *sample = listCache[ i ];
		if ( !sample ) {
			continue;
		}

		if ( com_purgeAll.GetBool() ) {
			sample->PurgeSoundSample();
		}

		sample->levelLoadReferenced = false;
	}

	soundCacheAllocator.FreeEmptyBaseBlocks();
}

/*
====================
EndLevelLoad

Free all samples marked as unused
====================
*/
void idSoundCache::EndLevelLoad() {
	int	useCount, purgeCount;
	common->Printf( "----- idSoundCache::EndLevelLoad -----\n" );

	insideLevelLoad = false;

	// purge the ones we don't need
	useCount = 0;
	purgeCount = 0;
	for ( int i = 0 ; i < listCache.Num() ; i++ ) {
		idSoundSample	*sample = listCache[ i ];
		if ( !sample ) {
			continue;
		}
		if ( sample->purged ) {
			continue;
		}
		if ( !sample->levelLoadReferenced ) {
//			common->Printf( "Purging %s\n", sample->name.c_str() );
			purgeCount += sample->objectMemSize;
			sample->PurgeSoundSample();
		} else {
			useCount += sample->objectMemSize;
		}
	}

	soundCacheAllocator.FreeEmptyBaseBlocks();

	common->Printf( "%5ik referenced\n", useCount / 1024 );
	common->Printf( "%5ik purged\n", purgeCount / 1024 );
}

/*
===================
idSoundCache::PrintMemInfo
===================
*/
void idSoundCache::PrintMemInfo( MemInfo_t *mi ) {
	int i, j, num = 0, total = 0;
	int *sortIndex;
	idFile *f;

	f = fileSystem->OpenFileWrite( mi->filebase + "_sounds.txt" );
	if ( !f ) {
		return;
	}

	// count
	for ( i = 0; i < listCache.Num(); i++, num++ ) {
		if ( !listCache[i] ) {
			break;
		}
	}

	// sort first
	sortIndex = new int[num];

	for ( i = 0; i < num; i++ ) {
		sortIndex[i] = i;
	}

	for ( i = 0; i < num - 1; i++ ) {
		for ( j = i + 1; j < num; j++ ) {
			if ( listCache[sortIndex[i]]->objectMemSize < listCache[sortIndex[j]]->objectMemSize ) {
				int temp = sortIndex[i];
				sortIndex[i] = sortIndex[j];
				sortIndex[j] = temp;
			}
		}
	}

	// print next
	for ( i = 0; i < num; i++ ) {
		idSoundSample *sample = listCache[sortIndex[i]];

		// this is strange
		if ( !sample ) {
			continue;
		}

		total += sample->objectMemSize;
		f->Printf( "%s %s\n", idStr::FormatNumber( sample->objectMemSize ).c_str(), sample->name.c_str() );
	}

	mi->soundAssetsTotal = total;

	f->Printf( "\nTotal sound bytes allocated: %s\n", idStr::FormatNumber( total ).c_str() );
	fileSystem->CloseFile( f );
	delete[] sortIndex;
}


/*
==========================================================================

idSoundSample

==========================================================================
*/

/*
===================
idSoundSample::idSoundSample
===================
*/
idSoundSample::idSoundSample() {
	memset( &objectInfo, 0, sizeof(waveformatex_t) );
	objectSize = 0;
	objectMemSize = 0;
	nonCacheData = NULL;
	amplitudeData = NULL;
	openalBuffer = 0;
	hardwareBuffer = false;
	defaultSound = false;
	onDemand = false;
	purged = false;
	levelLoadReferenced = false;
}

/*
===================
idSoundSample::~idSoundSample
===================
*/
idSoundSample::~idSoundSample() {
	PurgeSoundSample();
}

/*
===================
idSoundSample::LengthIn44kHzSamples
===================
*/
int idSoundSample::LengthIn44kHzSamples( void ) const {
	// objectSize is samples
	if ( objectInfo.nSamplesPerSec == 11025 ) {
		return objectSize << 2;
	} else if ( objectInfo.nSamplesPerSec == 22050 ) {
		return objectSize << 1;
	} else {
		return objectSize << 0;
	}
}

/*
===================
idSoundSample::MakeDefault
===================
*/
void idSoundSample::MakeDefault( void ) {
	int		i;
	float	v;
	int		sample;

	memset( &objectInfo, 0, sizeof( objectInfo ) );

	objectInfo.nChannels = 1;
	objectInfo.wBitsPerSample = 16;
	objectInfo.nSamplesPerSec = 44100;

	objectSize = MIXBUFFER_SAMPLES * 2;
	objectMemSize = objectSize * sizeof( short );

	nonCacheData = (byte *)soundCacheAllocator.Alloc( objectMemSize );

	short *ncd = (short *)nonCacheData;

	for ( i = 0; i < MIXBUFFER_SAMPLES; i ++ ) {
		v = sin( idMath::PI * 2 * i / 64 );
		sample = v * 0x4000;
		ncd[i*2+0] = sample;
		ncd[i*2+1] = sample;
	}

	alGetError();
	alGenBuffers( 1, &openalBuffer );
	if ( alGetError() != AL_NO_ERROR ) {
		common->Error( "idSoundCache: error generating OpenAL hardware buffer" );
	}

	alGetError();
	alBufferData( openalBuffer, objectInfo.nChannels==1?AL_FORMAT_MONO16:AL_FORMAT_STEREO16, nonCacheData, objectMemSize, objectInfo.nSamplesPerSec );
	if ( alGetError() != AL_NO_ERROR ) {
		common->Warning( "idSoundCache: error loading data into OpenAL hardware buffer" );
		hardwareBuffer = false;
	} else {
		hardwareBuffer = true;
	}

	defaultSound = true;
}

/*
===================
idSoundSample::CheckForDownSample
===================
*/
void idSoundSample::CheckForDownSample( void ) {
	if ( !idSoundSystemLocal::s_force22kHz.GetBool() ) {
		return;
	}
	if ( objectInfo.wFormatTag != WAVE_FORMAT_TAG_PCM || objectInfo.nSamplesPerSec != 44100 ) {
		return;
	}
	int shortSamples = objectSize >> 1;
	short *converted = (short *)soundCacheAllocator.Alloc( shortSamples * sizeof( short ) );

	if ( objectInfo.nChannels == 1 ) {
		for ( int i = 0; i < shortSamples; i++ ) {
			converted[i] = ((short *)nonCacheData)[i*2];
		}
	} else {
		for ( int i = 0; i < shortSamples; i += 2 ) {
			converted[i+0] = ((short *)nonCacheData)[i*2+0];
			converted[i+1] = ((short *)nonCacheData)[i*2+1];
		}
	}
	soundCacheAllocator.Free( nonCacheData );
	nonCacheData = (byte *)converted;
	objectSize >>= 1;
	objectMemSize >>= 1;
	objectInfo.nAvgBytesPerSec >>= 1;
	objectInfo.nSamplesPerSec >>= 1;
}

/*
===================
idSoundSample::GetNewTimeStamp
===================
*/
ID_TIME_T idSoundSample::GetNewTimeStamp( void ) const {
	ID_TIME_T timestamp;

	fileSystem->ReadFile( name, NULL, &timestamp );
	if ( timestamp == FILE_NOT_FOUND_TIMESTAMP ) {
		idStr oggName = name;
		oggName.SetFileExtension( ".ogg" );
		fileSystem->ReadFile( oggName, NULL, &timestamp );
	}
	return timestamp;
}

#if defined(__vita__)
static bool PreparedSoundCacheName( const idSoundSample *sample, char *path, size_t pathSize ) {
	return S_PreparedSoundCacheName( sample->name.c_str(),
		idSoundSystemLocal::s_force22kHz.GetBool(),
		idSoundSystemLocal::s_realTimeDecoding.GetBool(),
		idSoundSystemLocal::s_decompressionLimit.GetInteger(), path, pathSize );
}

static bool LoadPreparedSound( idSoundSample *sample ) {
	char cacheName[128];
	if ( !PreparedSoundCacheName( sample, cacheName, sizeof(cacheName) ) ) {
		return false;
	}

	unsigned char header[PREPARED_SOUND_HEADER_SIZE];
	preparedSoundInfo_t info;
	byte *data = NULL;
	bool migrateLegacy = false;
	bool legacyCorrupt = false;
	unsigned char *packedContents = NULL;
	int packedLength = 0;
	if ( PreparedCachePack_Read(PREPARED_CACHE_PACK_SOUND, cacheName,
			&packedContents, &packedLength) ) {
		if ( packedLength >= PREPARED_SOUND_HEADER_SIZE &&
				S_ParsePreparedSoundHeader(packedContents, packedLength, &info) ) {
			data = (byte *)soundCacheAllocator.Alloc( (int)info.objectMemSize );
			if ( data ) memcpy(data, packedContents + PREPARED_SOUND_HEADER_SIZE, info.objectMemSize);
		}
		free(packedContents);
	}
	if ( !data ) {
		idFile *file = fileSystem->OpenFileRead( cacheName, false );
		if ( file ) {
			const bool validHeader = file->Read( header, sizeof(header) ) == sizeof(header) &&
				S_ParsePreparedSoundHeader( header, (unsigned int)file->Length(), &info );
			if ( validHeader ) {
				data = (byte *)soundCacheAllocator.Alloc( (int)info.objectMemSize );
				if ( data && file->Read(data, (int)info.objectMemSize) == (int)info.objectMemSize ) {
					migrateLegacy = true;
				} else if ( data ) {
					soundCacheAllocator.Free(data);
					data = NULL;
					legacyCorrupt = true;
				}
			} else {
				legacyCorrupt = true;
			}
			fileSystem->CloseFile(file);
			if ( legacyCorrupt ) fileSystem->RemoveFile(cacheName);
		}
	}
	if ( !data ) {
		return false;
	}
	if ( migrateLegacy ) {
		S_BuildPreparedSoundHeader(header, info);
		PreparedCachePack_Write(PREPARED_CACHE_PACK_SOUND, cacheName,
			header, sizeof(header), data, info.objectMemSize);
	}

	memset( &sample->objectInfo, 0, sizeof(sample->objectInfo) );
	sample->objectInfo.wFormatTag = (word)info.formatTag;
	sample->objectInfo.nChannels = (word)info.channels;
	sample->objectInfo.nSamplesPerSec = info.samplesPerSec;
	sample->objectInfo.nAvgBytesPerSec = info.avgBytesPerSec;
	sample->objectInfo.nBlockAlign = (word)info.blockAlign;
	sample->objectInfo.wBitsPerSample = (word)info.bitsPerSample;
	sample->objectInfo.cbSize = (word)info.extraSize;
	sample->objectSize = (int)info.objectSize;
	sample->objectMemSize = (int)info.objectMemSize;
	sample->nonCacheData = data;
	sample->timestamp = 0;
	return true;
}

static void WritePreparedSound( const idSoundSample *sample ) {
	char cacheName[128];
	if ( !sample->nonCacheData || sample->objectMemSize <= 0 ||
			!PreparedSoundCacheName( sample, cacheName, sizeof(cacheName) ) ) return;

	preparedSoundInfo_t info;
	info.formatTag = sample->objectInfo.wFormatTag;
	info.channels = sample->objectInfo.nChannels;
	info.samplesPerSec = sample->objectInfo.nSamplesPerSec;
	info.avgBytesPerSec = sample->objectInfo.nAvgBytesPerSec;
	info.blockAlign = sample->objectInfo.nBlockAlign;
	info.bitsPerSample = sample->objectInfo.wBitsPerSample;
	info.extraSize = sample->objectInfo.cbSize;
	info.objectSize = sample->objectSize;
	info.objectMemSize = sample->objectMemSize;
	if ( info.objectMemSize > PREPARED_SOUND_MAX_DATA_SIZE ) return;

	unsigned char header[PREPARED_SOUND_HEADER_SIZE];
	S_BuildPreparedSoundHeader( header, info );
	if ( PreparedCachePack_Write(PREPARED_CACHE_PACK_SOUND, cacheName,
			header, sizeof(header), sample->nonCacheData, sample->objectMemSize) ) return;
	idFile *file = fileSystem->OpenFileWrite( cacheName, "fs_savepath" );
	if ( !file ) {
		return;
	}
	const bool ok = file->Write( header, sizeof(header) ) == sizeof(header) &&
		file->Write( sample->nonCacheData, sample->objectMemSize ) == sample->objectMemSize;
	fileSystem->CloseFile( file );
	if ( !ok ) {
		fileSystem->RemoveFile( cacheName );
	}
}
#endif

/*
===================
idSoundSample::Load

Loads based on name, possibly doing a MakeDefault if necessary
===================
*/
void idSoundSample::Load( void ) {
	defaultSound = false;
	purged = false;
	hardwareBuffer = false;

	bool prepared = false;
#if defined(__vita__)
	prepared = LoadPreparedSound( this );
#endif

	idWaveFile	fh;
	if ( !prepared ) {
		timestamp = GetNewTimeStamp();

		if ( timestamp == FILE_NOT_FOUND_TIMESTAMP ) {
			common->Warning( "Couldn't load sound '%s' using default", name.c_str() );
			MakeDefault();
			return;
		}

		waveformatex_t info;
		if ( fh.Open( name, &info ) == -1 ) {
			common->Warning( "Couldn't load sound '%s' using default", name.c_str() );
			MakeDefault();
			return;
		}

		if ( info.nChannels != 1 && info.nChannels != 2 ) {
			common->Warning( "idSoundSample: %s has %i channels, using default", name.c_str(), info.nChannels );
			fh.Close();
			MakeDefault();
			return;
		}

		if ( info.wBitsPerSample != 16 ) {
			common->Warning( "idSoundSample: %s is %dbits, expected 16bits using default", name.c_str(), info.wBitsPerSample );
			fh.Close();
			MakeDefault();
			return;
		}

		if ( info.nSamplesPerSec != 44100 && info.nSamplesPerSec != 22050 && info.nSamplesPerSec != 11025 ) {
			common->Warning( "idSoundCache: %s is %dHz, expected 11025, 22050 or 44100 Hz. Using default", name.c_str(), info.nSamplesPerSec );
			fh.Close();
			MakeDefault();
			return;
		}

		objectInfo = info;
		objectSize = fh.GetOutputSize();
		objectMemSize = fh.GetMemorySize();
		if ( objectMemSize <= 0 ) {
			common->Warning( "idSoundCache: invalid data size %d for %s", objectMemSize, name.c_str() );
			fh.Close();
			MakeDefault();
			return;
		}

		nonCacheData = (byte *)soundCacheAllocator.Alloc( objectMemSize );
		if ( !nonCacheData || fh.Read( nonCacheData, objectMemSize, NULL ) != objectMemSize ) {
			if ( nonCacheData ) soundCacheAllocator.Free( nonCacheData );
			nonCacheData = NULL;
			fh.Close();
			common->Warning( "idSoundCache: short read loading %s", name.c_str() );
			MakeDefault();
			return;
		}

		// Cache the representation after optional downsampling, before OpenAL upload.
		CheckForDownSample();
		fh.Close();
#if defined(__vita__)
		WritePreparedSound( this );
#endif
	}

	// create hardware audio buffers
	// PCM loads directly
	if ( objectInfo.wFormatTag == WAVE_FORMAT_TAG_PCM ) {
		alGetError();
		alGenBuffers( 1, &openalBuffer );
		if ( alGetError() != AL_NO_ERROR )
			common->Error( "idSoundCache: error generating OpenAL hardware buffer" );
		if ( alIsBuffer( openalBuffer ) ) {
			alGetError();
			alBufferData( openalBuffer, objectInfo.nChannels==1?AL_FORMAT_MONO16:AL_FORMAT_STEREO16, nonCacheData, objectMemSize, objectInfo.nSamplesPerSec );
			if ( alGetError() != AL_NO_ERROR ) {
				common->Warning( "idSoundCache: error loading data into OpenAL hardware buffer" );
				hardwareBuffer = false;
			} else {
				hardwareBuffer = true;
			}
		}
	}

	// OGG decompressed at load time (when smaller than s_decompressionLimit seconds, 6 seconds by default)
	if ( objectInfo.wFormatTag == WAVE_FORMAT_TAG_OGG ) {
		if ( ( objectSize < ( ( int ) objectInfo.nSamplesPerSec * idSoundSystemLocal::s_decompressionLimit.GetInteger() ) ) ) {
			alGetError();
			alGenBuffers( 1, &openalBuffer );
			if ( alGetError() != AL_NO_ERROR )
				common->Error( "idSoundCache: error generating OpenAL hardware buffer" );
			if ( alIsBuffer( openalBuffer ) ) {
				idSampleDecoder *decoder = idSampleDecoder::Alloc();
				float *destData = (float *)soundCacheAllocator.Alloc( ( LengthIn44kHzSamples() + 1 ) * sizeof( float ) );

				// Decoder *always* outputs 44 kHz data
				decoder->Decode( this, 0, LengthIn44kHzSamples(), destData );

				// Downsample back to original frequency (save memory)
				if ( objectInfo.nSamplesPerSec == 11025 ) {
					for ( int i = 0; i < objectSize; i++ ) {
						if ( destData[i*4] < -32768.0f )
							((short *)destData)[i] = -32768;
						else if ( destData[i*4] > 32767.0f )
							((short *)destData)[i] = 32767;
						else
							((short *)destData)[i] = idMath::FtoiFast( destData[i*4] );
					}
				} else if ( objectInfo.nSamplesPerSec == 22050 ) {
					for ( int i = 0; i < objectSize; i++ ) {
						if ( destData[i*2] < -32768.0f )
							((short *)destData)[i] = -32768;
						else if ( destData[i*2] > 32767.0f )
							((short *)destData)[i] = 32767;
						else
							((short *)destData)[i] = idMath::FtoiFast( destData[i*2] );
					}
				} else {
					for ( int i = 0; i < objectSize; i++ ) {
						if ( destData[i] < -32768.0f )
							((short *)destData)[i] = -32768;
						else if ( destData[i] > 32767.0f )
							((short *)destData)[i] = 32767;
						else
							((short *)destData)[i] = idMath::FtoiFast( destData[i] );
					}
				}

				alGetError();
				alBufferData( openalBuffer, objectInfo.nChannels==1?AL_FORMAT_MONO16:AL_FORMAT_STEREO16, destData, objectSize * sizeof( short ), objectInfo.nSamplesPerSec );
				if ( alGetError() != AL_NO_ERROR ) {
					common->Warning( "idSoundCache: error loading data into OpenAL hardware buffer" );
					hardwareBuffer = false;
				} else {
					hardwareBuffer = true;
				}

				soundCacheAllocator.Free( (byte *)destData );
				idSampleDecoder::Free( decoder );
			}
		}
	}

}

/*
===================
idSoundSample::PurgeSoundSample
===================
*/
void idSoundSample::PurgeSoundSample() {
	purged = true;

	alGetError();
	alDeleteBuffers( 1, &openalBuffer );
	if ( alGetError() != AL_NO_ERROR ) {
		common->Warning( "idSoundCache: error unloading data from OpenAL hardware buffer" );
	}

	openalBuffer = 0;
	hardwareBuffer = false;

	if ( amplitudeData ) {
		soundCacheAllocator.Free( amplitudeData );
		amplitudeData = NULL;
	}

	if ( nonCacheData ) {
		soundCacheAllocator.Free( nonCacheData );
		nonCacheData = NULL;
	}
}

/*
===================
idSoundSample::Reload
===================
*/
void idSoundSample::Reload( bool force ) {
	if ( !force ) {
		ID_TIME_T newTimestamp;

		// check the timestamp
		newTimestamp = GetNewTimeStamp();

		if ( newTimestamp == FILE_NOT_FOUND_TIMESTAMP ) {
			if ( !defaultSound ) {
				common->Warning( "Couldn't load sound '%s' using default", name.c_str() );
				MakeDefault();
			}
			return;
		}
		if ( newTimestamp == timestamp ) {
			return;	// don't need to reload it
		}
	}

	common->Printf( "reloading %s\n", name.c_str() );
	PurgeSoundSample();
	Load();
}

/*
===================
idSoundSample::FetchFromCache

Returns true on success.
===================
*/
bool idSoundSample::FetchFromCache( int offset, const byte **output, int *position, int *size, const bool allowIO ) {
	offset &= 0xfffffffe;

	if ( objectSize == 0 || offset < 0 || offset > objectSize * (int)sizeof( short ) || !nonCacheData ) {
		return false;
	}

	if ( output ) {
		*output = nonCacheData + offset;
	}
	if ( position ) {
		*position = 0;
	}
	if ( size ) {
		*size = objectSize * sizeof( short ) - offset;
		if ( *size > SCACHE_SIZE ) {
			*size = SCACHE_SIZE;
		}
	}
	return true;
}
