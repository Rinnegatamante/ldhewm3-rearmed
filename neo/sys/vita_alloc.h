#ifndef __VITA_ALLOC_H__
#define __VITA_ALLOC_H__

// Call only after vitaGL has registered the newlib heap and initialized its pools.
void Sys_EnableVitaGLAllocators();

#endif
