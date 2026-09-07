/*
   this is from zlib-1.2.7/contrib/minizip/

   ioapi.h -- IO base function header for compress/uncompress .zip
   part of the MiniZip project - ( http://www.winimage.com/zLibDll/minizip.html )

         Copyright (C) 1998-2010 Gilles Vollant (minizip) ( http://www.winimage.com/zLibDll/minizip.html )

         Modifications for Zip64 support
         Copyright (C) 2009-2010 Mathias Svensson ( http://result42.com )

         For more info read MiniZip_info.txt

*/

#if defined(_WIN32) && (!(defined(_CRT_SECURE_NO_WARNINGS)))
        #define _CRT_SECURE_NO_WARNINGS
#endif

#if defined(__APPLE__) || defined(IOAPI_NO_64) || defined(__OpenBSD__) || defined(__ANDROID__)
// In darwin and perhaps other BSD variants off_t is a 64 bit value, hence no need for specific 64 bit functions
#define FOPEN_FUNC(filename, mode) fopen(filename, mode)
#define FTELLO_FUNC(stream) ftello(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko(stream, offset, origin)
#else
#define FOPEN_FUNC(filename, mode) fopen64(filename, mode)
#define FTELLO_FUNC(stream) ftello64(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)
#endif


#include "ioapi.h"

#if defined(__vita__)
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <stdint.h>
#include <string.h>

#define SCEIO_READ_BUFFER_SIZE (64 * 1024)

typedef struct sceio_shared_file_s {
    SceUID fd;
    SceOff size;
    volatile int references;
} sceio_shared_file_t;

typedef struct sceio_stream_s {
    sceio_shared_file_t *shared;
    SceOff position;
    int error;
    unsigned char *read_buffer;
    SceOff read_buffer_start;
    SceSize read_buffer_length;
} sceio_stream_t;

static voidpf ZCALLBACK sceio_open64_file_func(voidpf opaque, const void* filename, int mode)
{
    int flags;
    SceUID fd;
    sceio_shared_file_t *shared;
    sceio_stream_t *stream;
    (void)opaque;
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) == ZLIB_FILEFUNC_MODE_READ)
        flags = SCE_O_RDONLY;
    else if (mode & ZLIB_FILEFUNC_MODE_CREATE)
        flags = SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC;
    else
        flags = SCE_O_RDWR;
    if (filename == NULL)
        return NULL;
    fd = sceIoOpen((const char *)filename, flags, 0777);
    if (fd < 0)
        return NULL;
    shared = (sceio_shared_file_t *)malloc(sizeof(*shared));
    stream = (sceio_stream_t *)malloc(sizeof(*stream));
    if (shared == NULL || stream == NULL) {
        sceIoClose(fd);
        free(shared);
        free(stream);
        return NULL;
    }
    shared->fd = fd;
    shared->references = 1;
    stream->shared = shared;
    stream->position = 0;
    stream->read_buffer = NULL;
    stream->read_buffer_start = 0;
    stream->read_buffer_length = 0;
    {
        SceIoStat stat;
        if (sceIoGetstatByFd(fd, &stat) < 0) {
            sceIoClose(fd);
            free(shared);
            free(stream);
            return NULL;
        }
        shared->size = stat.st_size;
    }
    stream->error = 0;
    return stream;
}

voidpf sceio_clone_file(voidpf file)
{
    sceio_stream_t *source = (sceio_stream_t *)file;
    sceio_stream_t *stream;
    if (source == NULL || source->shared == NULL)
        return NULL;
    stream = (sceio_stream_t *)malloc(sizeof(*stream));
    if (stream == NULL)
        return NULL;
    __sync_add_and_fetch(&source->shared->references, 1);
    stream->shared = source->shared;
    stream->position = 0;
    stream->error = 0;
    stream->read_buffer = NULL;
    stream->read_buffer_start = 0;
    stream->read_buffer_length = 0;
    return stream;
}

static uLong ZCALLBACK sceio_read_file_func(voidpf opaque, voidpf file, void* buf, uLong size)
{
    sceio_stream_t *stream = (sceio_stream_t *)file;
    unsigned char *out = (unsigned char *)buf;
    uLong total = 0;
    (void)opaque;

    while (total < size && stream->position < stream->shared->size) {
        const SceOff buffer_end = stream->read_buffer_start + stream->read_buffer_length;
        if (stream->read_buffer != NULL &&
            stream->position >= stream->read_buffer_start && stream->position < buffer_end) {
            const SceSize buffer_offset = (SceSize)(stream->position - stream->read_buffer_start);
            const SceSize available = stream->read_buffer_length - buffer_offset;
            const uLong remaining = size - total;
            const SceSize amount = available < remaining ? available : (SceSize)remaining;
            memcpy(out + total, stream->read_buffer + buffer_offset, amount);
            stream->position += amount;
            total += amount;
            continue;
        }

        if (size - total >= SCEIO_READ_BUFFER_SIZE) {
            int result = sceIoPread(stream->shared->fd, out + total,
                                    (SceSize)(size - total), stream->position);
            if (result < 0) {
                stream->error = result;
                break;
            }
            if (result == 0)
                break;
            stream->position += result;
            total += (uLong)result;
            continue;
        }

        /* The ZIP central directory is parsed through many 1-4 byte reads.
           Read it in blocks so each logical read does not become a syscall. */
        if (stream->read_buffer == NULL) {
            stream->read_buffer = (unsigned char *)malloc(SCEIO_READ_BUFFER_SIZE);
            if (stream->read_buffer == NULL) {
                stream->error = -1;
                break;
            }
        }

        stream->read_buffer_start = stream->position;
        {
            SceOff bytes_left = stream->shared->size - stream->position;
            SceSize refill_size = bytes_left < SCEIO_READ_BUFFER_SIZE ?
                (SceSize)bytes_left : (SceSize)SCEIO_READ_BUFFER_SIZE;
            int result = sceIoPread(stream->shared->fd, stream->read_buffer,
                                    refill_size, stream->position);
            if (result < 0) {
                stream->error = result;
                stream->read_buffer_length = 0;
                break;
            }
            stream->read_buffer_length = (SceSize)result;
            if (result == 0)
                break;
        }
    }
    return total;
}

static uLong ZCALLBACK sceio_write_file_func(voidpf opaque, voidpf file, const void* buf, uLong size)
{
    sceio_stream_t *stream = (sceio_stream_t *)file;
    SceSSize result;
    (void)opaque;
    stream->read_buffer_length = 0;
    result = sceIoPwrite(stream->shared->fd, buf, (SceSize)size, stream->position);
    if (result < 0) {
        stream->error = (int)result;
        return 0;
    }
    stream->position += result;
    if (stream->position > stream->shared->size)
        stream->shared->size = stream->position;
    return (uLong)result;
}

static ZPOS64_T ZCALLBACK sceio_tell64_file_func(voidpf opaque, voidpf file)
{
    sceio_stream_t *stream = (sceio_stream_t *)file;
    (void)opaque;
    return (ZPOS64_T)stream->position;
}

static long ZCALLBACK sceio_seek64_file_func(voidpf opaque, voidpf file, ZPOS64_T offset, int origin)
{
    sceio_stream_t *stream = (sceio_stream_t *)file;
    SceOff target;
    const SceOff relative = (SceOff)(int64_t)offset;
    (void)opaque;
    switch (origin) {
    case ZLIB_FILEFUNC_SEEK_CUR: target = stream->position + relative; break;
    case ZLIB_FILEFUNC_SEEK_END: target = stream->shared->size + relative; break;
    case ZLIB_FILEFUNC_SEEK_SET: target = relative; break;
    default: return -1;
    }
    if (target < 0 || target > stream->shared->size) {
        stream->error = -1;
        return -1;
    }
    stream->position = target;
    return 0;
}

static int ZCALLBACK sceio_close_file_func(voidpf opaque, voidpf file)
{
    sceio_stream_t *stream = (sceio_stream_t *)file;
    sceio_shared_file_t *shared = stream->shared;
    int result = 0;
    (void)opaque;
    free(stream->read_buffer);
    free(stream);
    if (__sync_sub_and_fetch(&shared->references, 1) == 0) {
        result = sceIoClose(shared->fd);
        free(shared);
    }
    return result;
}

static int ZCALLBACK sceio_error_file_func(voidpf opaque, voidpf file)
{
    (void)opaque;
    return ((sceio_stream_t *)file)->error;
}

void fill_sceio64_filefunc(zlib_filefunc64_def* filefunc)
{
    filefunc->zopen64_file = sceio_open64_file_func;
    filefunc->zread_file = sceio_read_file_func;
    filefunc->zwrite_file = sceio_write_file_func;
    filefunc->ztell64_file = sceio_tell64_file_func;
    filefunc->zseek64_file = sceio_seek64_file_func;
    filefunc->zclose_file = sceio_close_file_func;
    filefunc->zerror_file = sceio_error_file_func;
    filefunc->opaque = NULL;
}
#endif

voidpf call_zopen64 (const zlib_filefunc64_32_def* pfilefunc,const void*filename,int mode)
{
    if (pfilefunc->zfile_func64.zopen64_file != NULL)
        return (*(pfilefunc->zfile_func64.zopen64_file)) (pfilefunc->zfile_func64.opaque,filename,mode);
    else
    {
        return (*(pfilefunc->zopen32_file))(pfilefunc->zfile_func64.opaque,(const char*)filename,mode);
    }
}

long call_zseek64 (const zlib_filefunc64_32_def* pfilefunc,voidpf filestream, ZPOS64_T offset, int origin)
{
    if (pfilefunc->zfile_func64.zseek64_file != NULL)
        return (*(pfilefunc->zfile_func64.zseek64_file)) (pfilefunc->zfile_func64.opaque,filestream,offset,origin);
    else
    {
        uLong offsetTruncated = (uLong)offset;
        if (offsetTruncated != offset)
            return -1;
        else
            return (*(pfilefunc->zseek32_file))(pfilefunc->zfile_func64.opaque,filestream,offsetTruncated,origin);
    }
}

ZPOS64_T call_ztell64 (const zlib_filefunc64_32_def* pfilefunc,voidpf filestream)
{
    if (pfilefunc->zfile_func64.zseek64_file != NULL)
        return (*(pfilefunc->zfile_func64.ztell64_file)) (pfilefunc->zfile_func64.opaque,filestream);
    else
    {
        uLong tell_uLong = (*(pfilefunc->ztell32_file))(pfilefunc->zfile_func64.opaque,filestream);
        if ((tell_uLong) == MAXU32)
            return (ZPOS64_T)-1;
        else
            return tell_uLong;
    }
}

void fill_zlib_filefunc64_32_def_from_filefunc32(zlib_filefunc64_32_def* p_filefunc64_32,const zlib_filefunc_def* p_filefunc32)
{
    p_filefunc64_32->zfile_func64.zopen64_file = NULL;
    p_filefunc64_32->zopen32_file = p_filefunc32->zopen_file;
    p_filefunc64_32->zfile_func64.zerror_file = p_filefunc32->zerror_file;
    p_filefunc64_32->zfile_func64.zread_file = p_filefunc32->zread_file;
    p_filefunc64_32->zfile_func64.zwrite_file = p_filefunc32->zwrite_file;
    p_filefunc64_32->zfile_func64.ztell64_file = NULL;
    p_filefunc64_32->zfile_func64.zseek64_file = NULL;
    p_filefunc64_32->zfile_func64.zclose_file = p_filefunc32->zclose_file;
    p_filefunc64_32->zfile_func64.zerror_file = p_filefunc32->zerror_file;
    p_filefunc64_32->zfile_func64.opaque = p_filefunc32->opaque;
    p_filefunc64_32->zseek32_file = p_filefunc32->zseek_file;
    p_filefunc64_32->ztell32_file = p_filefunc32->ztell_file;
}



static voidpf  ZCALLBACK fopen_file_func OF((voidpf opaque, const char* filename, int mode));
static uLong   ZCALLBACK fread_file_func OF((voidpf opaque, voidpf stream, void* buf, uLong size));
static uLong   ZCALLBACK fwrite_file_func OF((voidpf opaque, voidpf stream, const void* buf,uLong size));
static ZPOS64_T ZCALLBACK ftell64_file_func OF((voidpf opaque, voidpf stream));
static long    ZCALLBACK fseek64_file_func OF((voidpf opaque, voidpf stream, ZPOS64_T offset, int origin));
static int     ZCALLBACK fclose_file_func OF((voidpf opaque, voidpf stream));
static int     ZCALLBACK ferror_file_func OF((voidpf opaque, voidpf stream));

static voidpf ZCALLBACK fopen_file_func (voidpf opaque, const char* filename, int mode)
{
    FILE* file = NULL;
    const char* mode_fopen = NULL;
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
        mode_fopen = "rb";
    else
    if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
        mode_fopen = "r+b";
    else
    if (mode & ZLIB_FILEFUNC_MODE_CREATE)
        mode_fopen = "wb";

    if ((filename!=NULL) && (mode_fopen != NULL))
        file = fopen(filename, mode_fopen);
    return file;
}

static voidpf ZCALLBACK fopen64_file_func (voidpf opaque, const void* filename, int mode)
{
    FILE* file = NULL;
    const char* mode_fopen = NULL;
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
        mode_fopen = "rb";
    else
    if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
        mode_fopen = "r+b";
    else
    if (mode & ZLIB_FILEFUNC_MODE_CREATE)
        mode_fopen = "wb";

    if ((filename!=NULL) && (mode_fopen != NULL))
        file = FOPEN_FUNC((const char*)filename, mode_fopen);
    return file;
}


static uLong ZCALLBACK fread_file_func (voidpf opaque, voidpf stream, void* buf, uLong size)
{
    uLong ret;
    ret = (uLong)fread(buf, 1, (size_t)size, (FILE *)stream);
    return ret;
}

static uLong ZCALLBACK fwrite_file_func (voidpf opaque, voidpf stream, const void* buf, uLong size)
{
    uLong ret;
    ret = (uLong)fwrite(buf, 1, (size_t)size, (FILE *)stream);
    return ret;
}

static long ZCALLBACK ftell_file_func (voidpf opaque, voidpf stream)
{
    long ret;
    ret = ftell((FILE *)stream);
    return ret;
}


static ZPOS64_T ZCALLBACK ftell64_file_func (voidpf opaque, voidpf stream)
{
    ZPOS64_T ret;
    ret = FTELLO_FUNC((FILE *)stream);
    return ret;
}

static long ZCALLBACK fseek_file_func (voidpf  opaque, voidpf stream, uLong offset, int origin)
{
    int fseek_origin=0;
    long ret;
    switch (origin)
    {
    case ZLIB_FILEFUNC_SEEK_CUR :
        fseek_origin = SEEK_CUR;
        break;
    case ZLIB_FILEFUNC_SEEK_END :
        fseek_origin = SEEK_END;
        break;
    case ZLIB_FILEFUNC_SEEK_SET :
        fseek_origin = SEEK_SET;
        break;
    default: return -1;
    }
    ret = 0;
    if (fseek((FILE *)stream, offset, fseek_origin) != 0)
        ret = -1;
    return ret;
}

static long ZCALLBACK fseek64_file_func (voidpf  opaque, voidpf stream, ZPOS64_T offset, int origin)
{
    int fseek_origin=0;
    long ret;
    switch (origin)
    {
    case ZLIB_FILEFUNC_SEEK_CUR :
        fseek_origin = SEEK_CUR;
        break;
    case ZLIB_FILEFUNC_SEEK_END :
        fseek_origin = SEEK_END;
        break;
    case ZLIB_FILEFUNC_SEEK_SET :
        fseek_origin = SEEK_SET;
        break;
    default: return -1;
    }
    ret = 0;

    if(FSEEKO_FUNC((FILE *)stream, offset, fseek_origin) != 0)
                        ret = -1;

    return ret;
}


static int ZCALLBACK fclose_file_func (voidpf opaque, voidpf stream)
{
    int ret;
    ret = fclose((FILE *)stream);
    return ret;
}

static int ZCALLBACK ferror_file_func (voidpf opaque, voidpf stream)
{
    int ret;
    ret = ferror((FILE *)stream);
    return ret;
}

void fill_fopen_filefunc (pzlib_filefunc_def)
  zlib_filefunc_def* pzlib_filefunc_def;
{
    pzlib_filefunc_def->zopen_file = fopen_file_func;
    pzlib_filefunc_def->zread_file = fread_file_func;
    pzlib_filefunc_def->zwrite_file = fwrite_file_func;
    pzlib_filefunc_def->ztell_file = ftell_file_func;
    pzlib_filefunc_def->zseek_file = fseek_file_func;
    pzlib_filefunc_def->zclose_file = fclose_file_func;
    pzlib_filefunc_def->zerror_file = ferror_file_func;
    pzlib_filefunc_def->opaque = NULL;
}

void fill_fopen64_filefunc (zlib_filefunc64_def*  pzlib_filefunc_def)
{
    pzlib_filefunc_def->zopen64_file = fopen64_file_func;
    pzlib_filefunc_def->zread_file = fread_file_func;
    pzlib_filefunc_def->zwrite_file = fwrite_file_func;
    pzlib_filefunc_def->ztell64_file = ftell64_file_func;
    pzlib_filefunc_def->zseek64_file = fseek64_file_func;
    pzlib_filefunc_def->zclose_file = fclose_file_func;
    pzlib_filefunc_def->zerror_file = ferror_file_func;
    pzlib_filefunc_def->opaque = NULL;
}
