#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <stdlib.h>
#include "lowpix.h"

#ifndef LP_ALLOC_CUSTOM
void* lp_alloc(void* ptr, size_t nsize)
{
	if (nsize == 0) { free(ptr); return NULL; }
	else return realloc(ptr, nsize);
}
#endif

#ifdef WIN32
struct LPFileMapI
{
	LPVOID mem;
	uint64_t size;
	HANDLE file, fmap;
};
struct LPFileMap* lp_mmap(const char* filename)
{
	HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); if (!hFile) return 0;
	LARGE_INTEGER fsize; if (!GetFileSizeEx(hFile, &fsize)) goto fail1;
	HANDLE hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, fsize.HighPart, fsize.LowPart, NULL); if (!hFileMap) goto fail1;
	LPVOID lpAddress = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0); if (!lpAddress) goto fail2;
	struct LPFileMapI* m = lp_alloc(0, sizeof(*m)); m->mem = lpAddress, m->size = fsize.QuadPart, m->file = hFile, m->fmap = hFileMap;
	return (struct LPFileMap*)m;
fail2:
	CloseHandle(hFileMap);
fail1:
	CloseHandle(hFile);
	return 0;
}
void lp_unmap(struct LPFileMap* fmap)
{
	struct LPFileMapI* m = (struct LPFileMapI*)fmap;
	UnmapViewOfFile(m->mem);
	CloseHandle(m->fmap);
	CloseHandle(m->file);
}
#else
struct LPFileMap
{
	void* mem;
	uint64_t size;
	int fd;
};
struct LPFileMap* lp_mmap(const char* filename)
{
	int fd = open(filename, O_RDONLY, 0); if (fd < 0) return 0;
	struct stat statbuf; if (fstat(fd, &statbuf) != 0) goto fail1;
	void* mem = mmap(0, statbuf.st_size, PROT_READ, MAP_PRIVATE | MAP_FILE, fd, 0); if (mem == MAP_FAILED) goto fail1;
	struct LPFileMap* m = lp_alloc(0, sizeof(*m)); m->mem = mem, m->size = statbuf.st_size, m->fd = fd;
	return (struct LPFileMap*)m;
fail1:
	close(fd);
	return 0;
}
void lp_unmap(struct LPFileMap* fmap)
{
	struct LPFileMapI* m = (struct LPFileMapI*)fmap;
	munmap(m->mem, (size_t)m->size);
	close(m->fd);
}
#endif
