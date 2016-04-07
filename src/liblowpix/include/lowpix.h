#ifndef LP_LOWPIX_H
#define LP_LOWPIX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define LP_ALIGN(x, a) (void*)(((uintptr_t)(x) + (a) - (uintptr_t)1) & ~((a) - (uintptr_t)1))
#define LP_MIN(a, b) (((a) < (b)) ? (a) : (b))


// MEM
// define LP_ALLOC_CUSTOM to override with your own
extern void* lp_alloc(void* ptr, size_t nsize);
extern void* lp_zalloc(size_t size); // init mem to 0

struct LPFileMap { void* mem; uint64_t size; };
extern struct LPFileMap* lp_mmap(const char* filename);
extern void lp_munmap(struct LPFileMap* fmap);

static inline uint16_t lp_read_u16_le(void* p)
{ uint8_t* s = (uint8_t*)p; uint16_t v = s[0] | s[1]<<8; return v; }
static inline uint32_t lp_read_u32_le(void* p)
{ uint8_t* s = (uint8_t*)p; uint32_t v = s[0] | s[1]<<8 | s[2]<<16 | s[3]<<24; return v; }
static inline uint16_t lp_read_u16_lep(void** p)
{ uint8_t* s = (uint8_t*)*p; uint16_t v = s[0] | s[1]<<8; *p = s + 2; return v; }
static inline uint32_t lp_read_u32_lep(void** p)
{ uint8_t* s = (uint8_t*)*p; uint32_t v = s[0] | s[1]<<8 | s[2]<<16 | s[3]<<24; *p = s + 4; return v; }

static inline uint16_t lp_read_u16_be(void* p)
{ uint8_t* s = (uint8_t*)p; uint16_t v = s[0]<<8 | s[1]; return v; }
static inline uint32_t lp_read_u32_be(void* p)
{ uint8_t* s = (uint8_t*)p; uint32_t v = s[0]<<24 | s[1]<<16 | s[2]<<8 | s[3]; return v; }


// CODEC
extern void* lp_cod_rle(void* data, size_t* data_sz);
extern void* lp_dec_rle(void* data, size_t* data_sz);
extern void* lp_cod_huf4(void* data, size_t* data_sz);
extern void* lp_cod_huf8(void* data, size_t* data_sz);
extern void* lp_cod_lz77(void* data, size_t* data_sz);
extern void* lp_dec_lz77(void* data, size_t* data_sz);


// PALETTE
struct LPPalette { uint32_t col_count; uint32_t col[1]; /* overallocated */ };
extern struct LPPalette* lp_pal_load(const char* fn, void* data, size_t sz);

#ifdef __cplusplus
}
#endif

#endif
