#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "lowpix.h"

// for parsing/writing foreign file formats, not for general editing
#define LP_PALCC_MAX (256)

#ifdef WIN32
#define LP_SECPATHSEP '\\'
#else
#define LP_SECPATHSEP '/'
#include <strings.h>
#define strnicmp strncasecmp
#endif

uint16_t lp_col5(uint32_t col)
{
	uint32_t 
		r = ((col&0xFF)*31 + 0x80) / 255,
		g = (((col>>8)&0xFF)*31 + 0x80) / 255,
		b = (((col>>16)&0xFF)*31 + 0x80) / 255;
	return (uint16_t)(r | g<<5 | b<<10);
}
uint32_t lp_col8(uint16_t col)
{
	uint32_t 
		r = ((col&31)*255 + 0xF) / 31,
		g = (((col>>5)&31)*255 + 0xF) / 31,
		b = (((col>>10)&31)*255 + 0xF) / 31;
	return (uint32_t)(r | g<<8 | b<<16);
}

static void lp_pal_save_bin(struct LPPalette* pal, FILE* f, const char* name, const char* h)
{
	for (uint32_t i = 0; i < pal->col_count; ++i)
	{
		uint8_t b[3] = { pal->col[i] & 0xFF, (pal->col[i]>>8) & 0xFF, (pal->col[i]>>16) & 0xFF };
		fwrite(b, 1, 3, f);
	}
}
static void lp_pal_save_act(struct LPPalette* pal, FILE* f, const char* name, const char* h)
{
	uint32_t i;
	for (i = 0; i < pal->col_count && i < 256; ++i)
	{
		uint8_t b[3] = { pal->col[i] & 0xFF, (pal->col[i]>>8) & 0xFF, (pal->col[i]>>16) & 0xFF };
		fwrite(b, 1, 3, f);
	}
	uint32_t v = 0;
	for (; i < 256; ++i)
		fwrite(&v, 1, 3, f);
}
static void lp_pal_save_gpl(struct LPPalette* pal, FILE* f, const char* name, const char* h)
{
	fprintf(f, "GIMP Palette\nName: %s\nColumns: 0\n#\n", name);
	for (uint32_t i = 0; i < pal->col_count; ++i)
		fprintf(f, "%*d %*d %*d Untitled\n", 3, pal->col[i] & 0xFF, 3, (pal->col[i]>>8) & 0xFF, 3, (pal->col[i]>>16) & 0xFF);
}
static void lp_pal_save_asm(struct LPPalette* pal, FILE* f, const char* name, const char* h)
{
	fprintf(f, "\t.section .rodata\n\t.align 2\n\t.global %s\n\t.hidden %s\n%s:", name, name, name);
	for (uint32_t i = 0; i < pal->col_count; ++i)
	{
		if (i%8 == 0) fprintf(f, "\n\t.hword ");
		fprintf(f, "0x%04X%s", lp_col5(pal->col[i]), (i+1)%8==0 || i == pal->col_count - 1 ? "" : ",");
	}
	FILE* fh = fopen(h, "wb");
	if (fh)
	{
		char uname[256]; for (int i = 0; i <= strlen(name); ++i) uname[i] = (char)toupper(name[i]);
		fprintf(fh, "#ifndef LPGEN_%s_H\n#define LPGEN_%s_H\n\n#define %s_size (%d)\nextern const u16 %s[0x%X];\n\n#endif\n",
			uname, uname, name, pal->col_count*2, name, pal->col_count);
		fclose(fh);
	}
}
static void lp_pal_save_c(struct LPPalette* pal, FILE* f, const char* name, const char* h)
{
	char uname[256]; for (int i = 0; i <= strlen(name); ++i) uname[i] = (char)toupper(name[i]);
	fprintf(f, "#ifndef LPGEN_%s_H\n#define LPGEN_%s_H\n\n#define %s_size (%d)\nextern const u16 %s[0x%X];\n\n#endif\n\n#ifdef %s_IMPLEMENTATION\n\nu16 %s[%d] = {",
			uname, uname, name, pal->col_count*2, name, pal->col_count, uname, name, pal->col_count);
	for (uint32_t i = 0; i < pal->col_count; ++i)
		fprintf(f, "%s0x%04X%s", i%8==0 ? "\n\t" : "", lp_col5(pal->col[i]), i < pal->col_count - 1 ? "," : "");
	fprintf(f, "\n};\n\n#endif\n");
}
int lp_pal_save(struct LPPalette* pal, const char* fn, enum LPPaletteFormat format)
{
	if (pal->col_count <= 0) return 0;
	char name[256];
	size_t len = strlen(fn), name_s = 0, name_e = len, i;
	for (size_t i = name_e; i >= 0; --i) { if (fn[i] == '.') name_e = i; if (fn[i] == '/' || fn[i] == LP_SECPATHSEP) { name_s = i+1; break; } }
	if (name_s >= name_e || name_e - name_s >= sizeof(name)) return 0;
	strncpy(name, fn + name_s, name_e - name_s);
	name[name_e - name_s] = 0;
	if (format == LP_PALETTEFORMAT_EXT)
	{
		if (name_e <= 0 || name_e >= len - 1) return 0;
		static const char* exts[] = { "bin", "act", "gpl", "s", "c" };
		format = LP_PALETTEFORMAT_BIN;
		for (int i = 0; i < sizeof(exts) / sizeof(*exts); ++i) { if (strncmp(exts[i], fn + name_e + 1, strlen(exts[i])) == 0) { format = (enum LPPaletteFormat)i; break; } }
	}
	FILE* f = fopen(fn, "wb"); if (!f) return 0;
	char* h = lp_alloc(0, strlen(fn)+3);
	strcpy(h, fn);
	for (i = len-1; i >= 0; --i) { if (h[i] == '.') { h[i+1] = 'h'; h[i+2] = 0; break; } }
	if (i < 0) h[len] = '.', h[len+1] = 'h', h[len+2] = 0;
	static void (*writers[])(struct LPPalette* pal, FILE* f, const char* name, const char* fn) = { lp_pal_save_bin, lp_pal_save_act, lp_pal_save_gpl, lp_pal_save_asm, lp_pal_save_c };
	writers[format](pal, f, name, h);
	lp_alloc(h, 0);
	fclose(f);
	return 1;
}

static struct LPPalette* lp_pal_load_pal(uint8_t* data, size_t sz) // microsoft .pal
{
	uint32_t cc = data[22] | (uint32_t)data[23] << 8;
	if (cc == 0 || sz < 24 + cc * 4) return 0;
	if (cc > LP_PALCC_MAX) cc = LP_PALCC_MAX;
	data += 24;
	struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[cc]));
	pal->col_count = cc;
	for (uint32_t i = 0; i < cc; ++i, data += 4)
		pal->col[i] = (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16; // 4th byte is flags, unused
	return pal;
}
static struct LPPalette* lp_pal_load_gpl(uint8_t* data, size_t sz) // gimp .gpl
{
	size_t i = 0;
	for (int ln = 0; i < sz && ln < 4; ++i) if (data[i] == '\n') ++ln;
	uint32_t cc = 0;
	for (size_t j = i; j < sz; ++j) if (data[j] == '\n') ++cc;
	if (cc > LP_PALCC_MAX) cc = LP_PALCC_MAX;
	struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[cc]));
	pal->col_count = cc;
	for (uint32_t c = 0; i < sz && c < cc; ++c, ++i)
	{
		int r, g, b;
		sscanf(data + i, "%d %d %d", &r, &g, &b);
		pal->col[c] = r | g << 8 | b << 16;
		for (; i < sz && data[i] != '\n'; ++i);
	}
	return pal;
}
static struct LPPalette* lp_pal_load_bin(uint8_t* data, size_t sz) // raw .bin
{
	uint32_t cc = (uint32_t)sz / 3; if (cc == 0) return 0;
	struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[cc]));
	pal->col_count = cc;
	for (uint32_t i = 0; i < cc; ++i, data += 3)
		pal->col[i] = (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16;
	return pal;
}
static struct LPPalette* lp_pal_load_act(uint8_t* data, size_t sz) // photoshop .act
{
	if (sz < 256 * 3) return 0;
	struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[256]));
	pal->col_count = 256;
	for (uint32_t i = 0; i < 256; ++i, data += 3)
		pal->col[i] = (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16;
	return pal;
}
static struct LPPalette* lp_pal_load_gif(uint8_t* data, size_t sz) // image .gif
{
	if (sz < 12 || (data[4] != '9' && data[4] != '7') || data[5] != 'a' || !(data[10] & 0x80)) return 0;
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
	data += 14 + lp_read_u32_le(data + 14);
	struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[cc]));
	pal->col_count = cc;
	for (uint32_t i = 0; i < cc; ++i, data += 4)
		pal->col[i] = (uint32_t)data[0] << 16 | (uint32_t)data[1] << 8 | (uint32_t)data[2];
	return pal;
}
static struct LPPalette* lp_pal_load_tga(uint8_t* data, size_t sz) // image .tga
{
	if (data[2] != 1 && data[2] != 9) return 0; // not palette based
	int i = 18 + lp_read_u16_le(data+3); if (sz <= i) return 0;
	uint32_t cc = lp_read_u16_le(data+5);
	if (cc > LP_PALCC_MAX) cc = LP_PALCC_MAX;
	int bpp = data[7];
	if (bpp != 15 && bpp != 16 && bpp != 24 && bpp != 32) return 0;
	data += i;
	struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[cc]));
	pal->col_count = cc;
	if (bpp == 15 || bpp == 16) // RGB 5-5-5
	{
		for (uint32_t i = 0; i < cc; ++i, data += 2)
		{
			uint32_t rgb = lp_read_u16_le(data);
			pal->col[i] = ((rgb>>10)&31)*255/31 | ((rgb>>5)&31)*255/31 << 8 | (rgb&31)*255/31 << 16;
		}
	}
	else
	{
		int inc = bpp == 24 ? 3 : 4;
		for (uint32_t i = 0; i < cc; ++i, data += inc)
			pal->col[i] = (uint32_t)data[0] << 16 | (uint32_t)data[1] << 8 | (uint32_t)data[2];
	}
	return pal;
}
struct LPPalette* lp_pal_load_i(const char* fn, void* data, size_t sz)
{
	// palette only formats
	if (sz > 24 && strncmp("RIFF", data, 4) == 0 && strncmp("PAL data", (uint8_t*)data + 8, 8) == 0) return lp_pal_load_pal(data, sz);
	if (sz > 12 && strncmp("GIMP Palette", data, 12) == 0) return lp_pal_load_gpl(data, sz);
	if (fn && strnicmp(".bin", &fn[strlen(fn) - 4], 4) == 0) return lp_pal_load_bin(data, sz);
	if (fn && strnicmp(".act", &fn[strlen(fn) - 4], 4) == 0) return lp_pal_load_act(data, sz);
	// image formats containing palette
	if (sz > 4 && strncmp("GIF8", data, 4) == 0) return lp_pal_load_gif(data, sz);
	if (sz > 8 && strncmp("\211PNG\r\n\032\n", data, 8) == 0) return lp_pal_load_png(data, sz);
	if (sz > 128 && ((uint8_t*)data)[0] == 10 && ((uint8_t*)data)[2] == 1) return lp_pal_load_pcx(data, sz);
	if (sz > 54 && strncmp("BM", data, 2) == 0) return lp_pal_load_bmp(data, sz);
	if (sz > 18 && ((uint8_t*)data)[1] == 1) return lp_pal_load_tga(data, sz);
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

struct LPPalette* lp_pal_clone(struct LPPalette* pal)
{
	struct LPPalette* npal = lp_alloc(0, offsetof(struct LPPalette, col[pal->col_count]));
	npal->col_count = pal->col_count;
	memcpy(npal->col, pal->col, pal->col_count * sizeof(*pal->col));
	return npal;
}

struct LPPalette* lp_pal_concat(struct LPPalette* pal1, struct LPPalette* pal2)
{
	uint32_t cc = pal1->col_count + pal2->col_count;
	struct LPPalette* pal = lp_alloc(0, offsetof(struct LPPalette, col[cc]));
	pal->col_count = cc;
	memcpy(pal->col, pal1->col, pal1->col_count * sizeof(*pal->col));
	memcpy(pal->col + pal1->col_count, pal2->col, pal2->col_count * sizeof(*pal->col));
	return pal;
}

struct LPPalette* lp_pal_unique(struct LPPalette* pal)
{
	struct LPPalette* npal = lp_alloc(0, offsetof(struct LPPalette, col[pal->col_count]));
	npal->col_count = 0;
	for (uint32_t i = 0; i < pal->col_count; ++i)
	{
		uint32_t j;
		for (j = 0; j < npal->col_count && pal->col[i] != npal->col[j]; ++j);
		if (j == npal->col_count) npal->col[npal->col_count++] = pal->col[i];
	}
	return lp_alloc(npal, offsetof(struct LPPalette, col[npal->col_count]));
}

struct LPPalette* lp_pal_restrict(struct LPPalette* pal)
{
	struct LPPalette* npal = lp_alloc(0, offsetof(struct LPPalette, col[pal->col_count]));
	npal->col_count = pal->col_count;
	for (uint32_t i = 0; i < pal->col_count; ++i)
		npal->col[i] = lp_col8(lp_col5(pal->col[i]));
	return npal;
}
