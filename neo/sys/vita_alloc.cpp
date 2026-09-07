#ifdef __vita__
#include "sys/vita_alloc.h"
#include <vitaGL.h>
#include <stdint.h>
#include <atomic>

static std::atomic<bool> vitaGLAllocatorsReady(false);

void Sys_EnableVitaGLAllocators() {
	vitaGLAllocatorsReady.store(true, std::memory_order_release);
}

extern "C" {
void *__real_malloc(uint32_t size);
void *__real_calloc(uint32_t count, uint32_t size);
void *__real_memalign(uint32_t alignment, uint32_t size);
void *__real_realloc(void *ptr, uint32_t size);
void __real_free(void *ptr);

void *__wrap_malloc(uint32_t size) {
	return vitaGLAllocatorsReady.load(std::memory_order_acquire) ? vglMalloc(size) : __real_malloc(size);
}
void *__wrap_calloc(uint32_t count, uint32_t size) {
	return vitaGLAllocatorsReady.load(std::memory_order_acquire) ? vglCalloc(count, size) : __real_calloc(count, size);
}
void *__wrap_memalign(uint32_t alignment, uint32_t size) {
	return vitaGLAllocatorsReady.load(std::memory_order_acquire) ? vglMemalign(alignment, size) : __real_memalign(alignment, size);
}
void *__wrap_realloc(void *ptr, uint32_t size) {
	return vitaGLAllocatorsReady.load(std::memory_order_acquire) ? vglRealloc(ptr, size) : __real_realloc(ptr, size);
}
void __wrap_free(void *ptr) {
	if ( vitaGLAllocatorsReady.load(std::memory_order_acquire) ) vglFree(ptr);
	else __real_free(ptr);
}
}
#endif
