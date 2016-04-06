#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "lowpix.h"

#define LP_PALCC_MAX (256)

static struct LPPalette* lp_pal_load_pal(uint8_t* data, size_t sz) // microsoft .pal
{
	uint32_t cc = data[22] | (uint32_t)data[23] << 8;
	if (cc == 0 || sz < 24 + cc * 4) return 0;
	if (cc > LP_PALCC_MAX) cc = LP_PALCC_MAX;
	data += 24;
	struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[cc]));
	pal->col_count = cc;
	for (uint32_t i = 0; i < cc; ++i, data += 4)
		pal->col[i] = (uint32_t)data[0] << 16 | (uint32_t)data[1] << 8 | (uint32_t)data[2]; // 4th byte is flags, unused
	return pal;
}
static struct LPPalette* lp_pal_load_gpl(uint8_t* data, size_t sz) // gimp .gpl
{
	size_t i = 0;
	for (int ln = 0; i < sz && ln < 4; ++i) if (data[i] == '\n') ++ln;
	uint32_t cc = 1;
	for (size_t j = i; j < sz; ++j) if (data[j] == '\n') ++cc;
	struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[cc]));
	pal->col_count = cc;
	for (int c = 0; i < sz; ++c, ++i)
	{
		int r, g, b;
		sscanf(data + i, "%d %d %d", &r, &g, &b);
		pal->col[c] = r | g << 8 | b << 16;
		for (; i < sz && data[i] != '\n'; ++i);
	}
	return pal;
}
static struct LPPalette* lp_pal_load_act(uint8_t* data, size_t sz) // photoshop .act
{
	if (sz != 256 * 3) return 0;
	struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[256]));
	pal->col_count = 256;
	for (uint32_t i = 0; i < 256; ++i, data += 3)
		pal->col[i] = (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16;
	return pal;
}
static struct LPPalette* lp_pal_load_gif(uint8_t* data, size_t sz) // image .gif
{
	if (sz < 12 || (data[5] != '9' && data[5] != '7') || data[6] != 'a' || !(data[10] & 0x80)) return 0;
	uint32_t cc = 2 << (data[10] & 7);
	data += 13;
	struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[cc]));
	pal->col_count = cc;
	for (uint32_t i = 0; i < cc; ++i, data += 3)
		pal->col[i] = (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16;
	return pal;
}
static struct LPPalette* lp_pal_load_png(uint8_t* data, size_t sz) // image .png
{
	int i = 8;
	while (i < sz)
	{
		uint32_t csz = lp_read_u32_be(data + i);
		if (strncmp("PLTE", data + i + 4, 4) == 0)
		{
			uint32_t cc = csz / 3;
			if (cc == 0) return 0;
			if (cc > LP_PALCC_MAX) cc = LP_PALCC_MAX;
			data += i + 8;
			struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[cc]));
			pal->col_count = cc;
			for (uint32_t i = 0; i < cc; ++i, data += 3)
				pal->col[i] = (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16;
			return pal;
		}
		i += 12 + csz;
	}
	return 0;
}
static struct LPPalette* lp_pal_load_pcx(uint8_t* data, size_t sz) // image .pcx
{
	int t = (int)data[3] * data[65];
	uint32_t cc;
	if (t == 1 || t == 4) data += 16, cc = 16;
	else if (t == 8 && sz > 769 && data[sz - 769] == 12) data += sz - 768, cc = 256;
	else return 0;
	struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[cc]));
	pal->col_count = cc;
	for (uint32_t i = 0; i < cc; ++i, data += 3)
		pal->col[i] = (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16;
	return pal;
}
static struct LPPalette* lp_pal_load_bmp(uint8_t* data, size_t sz) // image .bmp
{
	uint32_t cc = lp_read_u32_le(data + 46);
	if (cc == 0) return 0;
	data += 54;
	struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[cc]));
	pal->col_count = cc;
	for (uint32_t i = 0; i < cc; ++i, data += 4)
		pal->col[i] = (uint32_t)data[0] << 16 | (uint32_t)data[1] << 8 | (uint32_t)data[2];
	return pal;
}
struct LPPalette* lp_pal_load_i(const char* fn, void* data, size_t sz)
{
	// palette only formats
	if (sz > 24 && strncmp("RIFF", data, 4) == 0 && strncmp("PAL data", (uint8_t*)data + 8, 8) == 0) return lp_pal_load_pal(data, sz);
	if (sz > 12 && strncmp("GIMP Palette", data, 12) == 0) return lp_pal_load_gpl(data, sz);
	if (fn && strnicmp(".act", &fn[strlen(fn) - 4], 4) == 0) return lp_pal_load_act(data, sz);
	// image formats containing palette
	if (sz > 4 && strncmp("GIF8", data, 4) == 0) return lp_pal_load_gif(data, sz);
	if (sz > 8 && strncmp("\211PNG\r\n\032\n", data, 8) == 0) return lp_pal_load_png(data, sz);
	if (sz > 128 && ((uint8_t*)data)[0] == 10 && ((uint8_t*)data)[2] == 1) return lp_pal_load_pcx(data, sz);
	if (sz > 54 && strncmp("BM", data, 2) == 0) return lp_pal_load_bmp(data, sz);
	return 0;
}
struct LPPalette* lp_pal_load(const char* fn, void* data, size_t sz)
{
	struct LPFileMap* fmap = 0;
	if (fn && !data)
	{
		if (!(fmap = lp_mmap(fn))) return 0;
		data = fmap->mem, sz = fmap->size;
	}
	struct LPPalette* pal = lp_pal_load_i(fn, data, sz);
	if (fmap) lp_munmap(fmap);
	return pal;
}
