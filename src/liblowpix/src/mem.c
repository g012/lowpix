#include <stdlib.h>
#include "lowpix.h"

#ifndef LP_ALLOC_CUSTOM
void* lp_alloc(void* ptr, size_t nsize)
{
	if (nsize == 0) { free(ptr); return NULL; }
	else return realloc(ptr, nsize);
}
#endif
