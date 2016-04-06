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
		pal->col[i] = data[0] << 16 | data[1] << 8 | data[2]; // 4th byte is flags, unused
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
		pal->col[i] = data[0] | data[1] << 8 | data[2] << 16;
	return pal;
}
struct LPPalette* lp_pal_load(void* data, size_t sz, const char* fn)
{
	if (sz > 24 && strncmp("RIFF", data, 4) == 0 && strncmp("PAL data", (uint8_t*)data + 8, 8) == 0) return lp_pal_load_pal(data, sz);
	if (sz > 12 && strncmp("GIMP Palette", data, 12) == 0) return lp_pal_load_gpl(data, sz);
	if (fn && strnicmp(".act", &fn[strlen(fn) - 4], 4) == 0) lp_pal_load_act(data, sz);
	return 0;
}
