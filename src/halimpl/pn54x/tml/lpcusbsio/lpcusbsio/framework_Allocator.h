#ifndef FRAMEWORK_ALLOCATOR_H
#define FRAMEWORK_ALLOCATOR_H

#include <stddef.h>


void* framework_AllocMem(size_t size);
void  framework_FreeMem(void *ptr);

#endif //ndef FRAMEWORK_ALLOCATOR_H