#include <string.h>
#include "lowpix.h"

enum
{
	LP_CODEC_LZ77		= 0x10,
	LP_CODEC_HUFF		= 0x20,
	LP_CODEC_HUFF4		= 0x24,
	LP_CODEC_HUFF8		= 0x28,
	LP_CODEC_RLE		= 0x30,
	LP_CODEC_DIFF8		= 0x81,
	LP_CODEC_DIFF16		= 0x82,
};

/*************************************************************************
 * RLE - taken from GRIT
 *************************************************************************/

void* lp_cod_rle(void* data, size_t* data_sz)
{
	if (!data || !data_sz || *data_sz <= 0) return 0;
	uint32_t ii, rle, non;
	uint8_t curr, prev;

	uint32_t srcS = (uint32_t)*data_sz;
	uint8_t *srcD = data;

	// Annoyingly enough, rle _can_ end up being larger than
	// the original. A checker-board will do it for example.
	// if srcS is the size of the alternating pattern, then
	// the endresult will be 4 + srcS + (srcS+0x80-1)/0x80.
	uint32_t dstS = 8 + 2 * (srcS);
	uint8_t *dstD = (uint8_t*)lp_alloc(0, dstS), *dstL = dstD;

	prev = srcD[0];
	rle = non = 1;

	// NOTE! non will always be 1 more than the actual non-stretch
	// PONDER: why [1,srcS] ?? (to finish up the stretch)
	for (ii = 1; ii <= srcS; ii++)
	{
		if (ii != srcS)
			curr = srcD[ii];

		if (rle == 0x82 || ii == srcS)	// stop rle
			prev = ~curr;

		if (rle < 3 && (non + rle > 0x80 || ii == srcS))	// ** mini non
		{
			non += rle;
			dstL[0] = non - 2;
			memcpy(&dstL[1], &srcD[ii - non + 1], non - 1);
			dstL += non;
			non = rle = 1;
		}
		else if (curr == prev)		// ** start rle / non on hold
		{
			rle++;
			if (rle == 3 && non > 1)	// write non-1 bytes
			{
				dstL[0] = non - 2;
				memcpy(&dstL[1], &srcD[ii - non - 1], non - 1);
				dstL += non;
				non = 1;
			}
		}
		else						// ** rle end / non start
		{
			if (rle >= 3)	// write rle
			{
				dstL[0] = 0x80 | (rle - 3);
				dstL[1] = srcD[ii - 1];
				dstL += 2;
				non = 0;
				rle = 1;
			}
			non += rle;
			rle = 1;
		}
		prev = curr;
	}

	dstS = (uint32_t)(uintptr_t)LP_ALIGN(dstL - dstD, 4) + 4;

	uint8_t* dst = lp_alloc(0, dstS);

	dst[0] = LP_CODEC_RLE;
	dst[1] = (srcS >> 0) & 0xFF;
	dst[2] = (srcS >> 8) & 0xFF;
	dst[3] = (srcS >> 16) & 0xFF;

	memcpy(dst + 4, dstD, dstS - 4);
	lp_alloc(dstD, 0);

	*data_sz = dstS;
	return dst;
}

void* lp_dec_rle(void* data, size_t* data_sz)
{
	if (!data || !data_sz || *data_sz <= 0) return 0;

	// Get and check header word
	uint32_t header = lp_read_u32_lep(&data);
	if ((uint8_t)header != LP_CODEC_RLE) return 0;

	uint32_t ii, dstS = header >> 8, size = 0;
	uint8_t *srcL = data, *dstD = (uint8_t*)lp_alloc(0, dstS);

	for (ii = 0; ii < dstS; ii += size)
	{
		// Get header byte
		header = *srcL++;

		if (header & 0x80)		// compressed stint
		{
			size = LP_MIN((header&~0x80) + 3, dstS - ii);
			memset(&dstD[ii], *srcL++, size);
		}
		else				// noncompressed stint
		{
			size = LP_MIN(header + 1, dstS - ii);
			memcpy(&dstD[ii], srcL, size);
			srcL += size;
		}
	}

	*data_sz = dstS;
	return dstD;
} 

/*************************************************************************
 * HUFFMAN - taken from GRIT
 *************************************************************************/

struct LPCodecHuff
{
	uint32_t *gids, *gprobs, *gtiers;
	int32_t *gdad, *glson, *grson;
	uint8_t *gtable;
};

//! Sorts the ids to the frequency table
static void lp_cod_huff_hufapp(uint32_t ids[], const uint32_t probs[], uint32_t nn, uint32_t ii)
{
	uint32_t jj, kk;

	kk = ids[ii];
	//while(ii < nn/2)
	while (ii <= nn / 2)
	{
		//jj= 2*ii+1;
		jj = 2 * ii;
		if (jj < nn && probs[ids[jj]] > probs[ids[jj + 1]])
			jj++;
		if (probs[kk] <= probs[ids[jj]])
			break;
		ids[ii] = ids[jj];
		ii = jj;
	}
	ids[ii] = kk;
}

//! Gather the frequency table
static void lp_cod_huff_init_freqs(uint32_t freqs[], const void *srcv, int srcS, int srcB)
{
	int ii, jj, nn = srcS / 4, mm = 32 / srcB;
	int count = 1 << srcB;
	uint32_t mask = count - 1;
	uint32_t *srcD = (uint32_t*)srcv;

	memset(freqs, 0, count*sizeof(uint32_t));
	for (ii = 0; ii < nn; ii++)
		for (jj = 0; jj < mm; jj++)			// for all the sub-pixels
			freqs[(srcD[ii] >> (jj*srcB))&mask]++;
}


//! Runs recursively through the tree, fi//! lling the table.
// OK. It's just sickening how easy this turned out to be
static void lp_cod_huff_table_fill(struct LPCodecHuff* ctx, int id, int tier)
{
	// go left
	if (ctx->glson[id])
	{
		lp_cod_huff_table_fill(ctx, ctx->glson[id], tier + 1);

		uint32_t curr = (ctx->gtiers[tier + 1] - ctx->gtiers[tier] - 3) / 2;
		curr |= (ctx->glson[ctx->glson[id]] ? 0 : 0x80);
		curr |= (ctx->grson[ctx->grson[id]] ? 0 : 0x40);

		// register branch
		ctx->gtable[ctx->gtiers[tier]++] = (uint8_t)curr;
	}
	else
		// register leaf
		ctx->gtable[ctx->gtiers[tier]++] = id - 1;

	// move up and right
	id = ctx->gdad[id];
	if (id < 0)
		lp_cod_huff_table_fill(ctx, ctx->grson[-id], tier);
}

//! Main Huffman routine
static void* lp_cod_huff(void* data, size_t* data_sz, int srcB)
{
	if (!data || !data_sz || *data_sz <= 0) return 0;
	int ii, jj, kk;
	int nch = 1 << srcB, nodes = 2 * nch - 1;
	int srcS = (int)*data_sz;

	// build frequency table and used id list
	struct LPCodecHuff ctx = { 0 };
	uint32_t freqs[256], _ids[512], _probs[512];
	ctx.gids = _ids - 1;

	ctx.gprobs = _probs - 1;
	lp_cod_huff_init_freqs(freqs, data, srcS, srcB);
	memcpy(_probs, freqs, 256 * sizeof(uint32_t));
	memset(&_probs[256], 0, 256 * sizeof(uint32_t));

	int nids = 0;
	for (ii = 1; ii <= nch; ii++)
		if (ctx.gprobs[ii])
			ctx.gids[++nids] = ii;

	// --- build tree ---

	// first the 'real' nodes
	for (ii = nids; ii > 0; ii--)
		lp_cod_huff_hufapp(ctx.gids, ctx.gprobs, nids, ii);

	int _lson[512], _rson[512], _dad[512];

	ctx.glson = _lson - 1;
	ctx.grson = _rson - 1;
	ctx.gdad = _dad - 1;
	memset(_lson, 0, 512 * sizeof(int));
	memset(_rson, 0, 512 * sizeof(int));
	memset(_dad, 0, 512 * sizeof(int));

	// now the composite nodes
	// ids[1] is always the lowest form
	ii = nch;
	while (nids)
	{
		jj = ctx.gids[1];
		ctx.gids[1] = ctx.gids[nids--];
		lp_cod_huff_hufapp(ctx.gids, ctx.gprobs, nids, 1);
		ctx.gprobs[++ii] = ctx.gprobs[ctx.gids[1]] + ctx.gprobs[jj];
		ctx.glson[ii] = ctx.gids[1];
		ctx.grson[ii] = jj;
		ctx.gdad[ctx.gids[1]] = -ii;	// negative indicates left node
		ctx.gdad[jj] = ctx.gids[1] = ii;
		lp_cod_huff_hufapp(ctx.gids, ctx.gprobs, nids, 1);
	}

	// ii-1 is root, not ii!!
	// Otherwise, you get one extra but useless bit in the codes
	nids = ii - 1;
	ctx.gdad[nids] = 0;

	// --- make the codes ---
	uint32_t lens[256], codes[256], code;

	// NOTE: gids is now trashed anyway, so keep track of the 
	// tiers there now
	int maxlen = 0;
	ctx.gtiers = _ids;
	memset(ctx.gtiers, 0, 256 * sizeof(uint32_t));

	for (ii = 0; ii < nch; ii++)
	{
		if (ctx.gprobs[ii + 1] == 0)
			continue;

		kk = ctx.gdad[ii + 1];
		code = 0;
		for (jj = 0; kk; jj++)
		{
			if (kk > 0)
				code |= 1 << jj;
			else
				kk = -kk;
			kk = ctx.gdad[kk];
		}
		codes[ii] = code;
		if (jj >= 32)		// codes are too long, FAIL!
			return 0;
		lens[ii] = jj;

		// tier tracker:
		jj++;
		ctx.gtiers[jj]++;
		if (jj > maxlen)
			maxlen = jj;
	}

	// --- create the table --- (ack!)

	// finish up tier positions
	for (ii = maxlen - 1; ii >= 0; ii--)
		ctx.gtiers[ii] += ctx.gtiers[ii + 1] / 2;

	for (ii = 0; ii < maxlen; ii++)
		ctx.gtiers[ii + 1] += ctx.gtiers[ii];

	uint8_t table[512];
	ctx.gtable = table;
	memset(table, -1, 512);

	lp_cod_huff_table_fill(&ctx, nids, 0);

	// --- Encode the source data ---

	int dstS = srcS * 2;
	uint32_t mask = nch - 1;

	uint8_t *dstD = (uint8_t*)lp_alloc(0, dstS);
	uint32_t *srcL4 = (uint32_t*)data, *dstL4 = (uint32_t*)dstD;
	uint32_t buf, chunk = 0;
	int nn = srcS / 4, mm = 32 / srcB, len = 32;

	for (ii = 0; ii < nn; ii++)
	{
		buf = *srcL4++;
		for (jj = 0; jj < mm; jj++)
		{
			kk = (buf >> (jj*srcB)) & mask;
			len -= lens[kk];
			if (len < 0)	// goto new uint32_t
			{
				chunk |= codes[kk] >> (-len);
				*dstL4++ = chunk;
				len += 32;
				chunk = codes[kk] << len;
				dstS += 4;
			}
			else		// business as usual
				chunk |= codes[kk] << len;
		}
	}
	// don't forget the rest
	if (len != 32)
		*dstL4++ = chunk;
	dstS = (int)((uint8_t*)dstL4 - (uint8_t*)dstD);

	// --- put everything together ---
	// full size: header (4) + table size (1) + table (gtiers[maxlen]) + dstS

	len = ctx.gtiers[maxlen];
	dstS = 5 + len + dstS;
	dstS = (int)(uintptr_t)LP_ALIGN(dstS, 4);
	uint8_t* dst = lp_alloc(0, dstS);

	dst[0] = LP_CODEC_HUFF | srcB;
	dst[1] = (srcS >> 0) & 0xFF;
	dst[2] = (srcS >> 8) & 0xFF;
	dst[3] = (srcS >> 16) & 0xFF;

	dst[4] = (len - 1) / 2;
	memcpy(&dst[5], ctx.gtable, len);
	memcpy(&dst[5 + len], dstD, dstS - len - 5);
	lp_alloc(dstD, 0);

	*data_sz = dstS;
	return dst;
}
void* lp_cod_huf4(void* data, size_t* data_sz) { return lp_cod_huff(data, data_sz, 4); }
void* lp_cod_huf8(void* data, size_t* data_sz) { return lp_cod_huff(data, data_sz, 8); }


/*************************************************************************
 * LZ77 - taken from GRIT
 *************************************************************************/

// Define information for compression
//   (dont modify from 4096/18/2 if AGBCOMP format is required)
#define LP_LZ77_RING_MAX        4096   // size of ring buffer (12 bit)
#define LP_LZ77_FRAME_MAX         18   // upper limit for match_length
#define LP_LZ77_THRESHOLD          2   // encode string into position and length
//   if matched length is greater than this 
#define LP_LZ77_NIL         LP_LZ77_RING_MAX   // index for root of binary search trees 
#define LP_LZ77_TEXT_BUF_CLEAR     0   // byte to initialize the area before text_buf with
#define LP_LZ77_NMASK           (LP_LZ77_RING_MAX-1)  // for wrapping

// encoder context
struct LPCodecLZ77
{
	uint32_t codesize;  // code size counter

	// Ring buffer of size LP_LZ77_RING_MAX with extra LP_LZ77_FRAME_MAX-1 bytes to 
	// facilitate string comparison
	uint8_t text_buf[LP_LZ77_RING_MAX + LP_LZ77_FRAME_MAX - 1];
	int32_t match_position;  // global string match position
	int32_t match_length;  // global string match length

	// left & right children & parents -- These constitute binary search trees.
	int32_t lson[LP_LZ77_RING_MAX + 1], rson[LP_LZ77_RING_MAX + 256 + 1], dad[LP_LZ77_RING_MAX + 1];

	uint8_t *InBuf, *OutBuf;
	int32_t InSize, OutSize, InOffset;
};

/* lp_cod_lz77_inittree() **************************
initialize a binary search tree.

for i = 0 to LP_LZ77_RING_MAX - 1, rson[i] and lson[i] will be the right and
left children of node i.  These nodes need not be initialized.
also, dad[i] is the parent of node i.  These are initialized
to LP_LZ77_NIL (= LP_LZ77_RING_MAX), which stands for 'not used.'
for i = 0 to 255, rson[LP_LZ77_RING_MAX + i + 1] is the root of the tree
for strings that begin with character i.  These are
initialized to LP_LZ77_NIL.  Note there are 256 trees.
*/
static void lp_cod_lz77_inittree(struct LPCodecLZ77* ctx)
{
	int32_t i;
	for (i = LP_LZ77_RING_MAX + 1; i <= LP_LZ77_RING_MAX + 256; i++)
		ctx->rson[i] = LP_LZ77_NIL;
	for (i = 0; i < LP_LZ77_RING_MAX; i++)
		ctx->dad[i] = LP_LZ77_NIL;
}

/* lp_cod_lz77_insertnode() ************************
inserts string of length LP_LZ77_FRAME_MAX, text_buf[r..r+LP_LZ77_FRAME_MAX-1], into one of the
trees (text_buf[r]'th tree) and returns the longest-match position
and length via the global variables match_position and match_length.
if match_length = LP_LZ77_FRAME_MAX, then removes the old node in favor of the new
one, because the old one will be deleted sooner.
note r plays double role, as tree node and position in buffer.
*/
static void lp_cod_lz77_insertnode(struct LPCodecLZ77* ctx, int r)
{
	int32_t i, p, cmp, prev_length;
	uint8_t *key;

	cmp = 1;  key = &ctx->text_buf[r];  p = LP_LZ77_RING_MAX + 1 + key[0];
	ctx->rson[r] = ctx->lson[r] = LP_LZ77_NIL;
	prev_length = ctx->match_length = 0;
	for (; ; )
	{
		if (cmp >= 0)
		{
			if (ctx->rson[p] != LP_LZ77_NIL)
				p = ctx->rson[p];
			else
			{
				ctx->rson[p] = r;
				ctx->dad[r] = p;
				return;
			}
		}
		else
		{
			if (ctx->lson[p] != LP_LZ77_NIL)
				p = ctx->lson[p];
			else
			{
				ctx->lson[p] = r;
				ctx->dad[r] = p;
				return;
			}

		}
		for (i = 1; i < LP_LZ77_FRAME_MAX; i++)
			if ((cmp = key[i] - ctx->text_buf[p + i]) != 0)
				break;

		if (i > ctx->match_length)
		{
			// VRAM safety:
			// match_length= i ONLY if the matched position 
			// isn't the previous one (r-1)
			// for normal case, remove the if.
			// That's _IT_?!? Yup, that's it.
			if (p != ((r - 1)&LP_LZ77_NMASK))
			{
				ctx->match_length = i;
				ctx->match_position = p;
			}
			if (ctx->match_length >= LP_LZ77_FRAME_MAX)
				break;
		}
	}

	// Full length match, remove old node in favor of this one
	ctx->dad[r] = ctx->dad[p];
	ctx->lson[r] = ctx->lson[p];
	ctx->rson[r] = ctx->rson[p];
	ctx->dad[ctx->lson[p]] = r;
	ctx->dad[ctx->rson[p]] = r;
	if (ctx->rson[ctx->dad[p]] == p)
		ctx->rson[ctx->dad[p]] = r;
	else
		ctx->lson[ctx->dad[p]] = r;
	ctx->dad[p] = LP_LZ77_NIL;
}


/* lp_cod_lz77_deletenode() ************************
deletes node p from the tree.
*/
static void lp_cod_lz77_deletenode(struct LPCodecLZ77* ctx, int p)
{
	int32_t q;

	if (ctx->dad[p] == LP_LZ77_NIL)
		return;  /* not in tree */
	if (ctx->rson[p] == LP_LZ77_NIL)
		q = ctx->lson[p];
	else if (ctx->lson[p] == LP_LZ77_NIL)
		q = ctx->rson[p];
	else
	{
		q = ctx->lson[p];
		if (ctx->rson[q] != LP_LZ77_NIL)
		{
			do {
				q = ctx->rson[q];
			} while (ctx->rson[q] != LP_LZ77_NIL);

			ctx->rson[ctx->dad[q]] = ctx->lson[q];
			ctx->dad[ctx->lson[q]] = ctx->dad[q];
			ctx->lson[q] = ctx->lson[p];
			ctx->dad[ctx->lson[p]] = q;
		}
		ctx->rson[q] = ctx->rson[p];
		ctx->dad[ctx->rson[p]] = q;
	}

	ctx->dad[q] = ctx->dad[p];

	if (ctx->rson[ctx->dad[p]] == p)
		ctx->rson[ctx->dad[p]] = q;
	else
		ctx->lson[ctx->dad[p]] = q;

	ctx->dad[p] = LP_LZ77_NIL;
}


/* lp_cod_lz77_inchar() ****************************
get the next character from the input stream, or -1 for end of file.
*/
static int lp_cod_lz77_inchar(struct LPCodecLZ77* ctx)
{
	return (ctx->InOffset < ctx->InSize) ? ctx->InBuf[ctx->InOffset++] : -1;
}

void* lp_dec_lz77(void* data, size_t* data_sz)
{
	if (!data || !data_sz || *data_sz <= 0) return 0;

	// Get and check header word
	uint32_t header = lp_read_u32_lep(&data);
	if ((uint8_t)header != LP_CODEC_LZ77) return 0;

	uint32_t flags;
	int32_t ii, jj, dstS = header >> 8;
	uint8_t *srcL = data, *dstD = (uint8_t*)lp_alloc(0, dstS);

	for (ii = 0, jj = -1; ii < dstS; jj--)
	{
		if (jj < 0)				// Get block flags
		{
			flags = *srcL++;
			jj = 7;
		}

		if (flags >> jj & 1)		// Compressed stint
		{
			int count = (srcL[0] >> 4) + LP_LZ77_THRESHOLD + 1;
			int ofs = ((srcL[0] & 15) << 8 | srcL[1]) + 1;
			srcL += 2;
			while (count--)
			{
				dstD[ii] = dstD[ii - ofs];
				ii++;
			}
		}
		else					// Single byte from source
			dstD[ii++] = *srcL++;
	}

	*data_sz = (size_t)dstS;
	return dstD;
}

void* lp_cod_lz77(void* data, size_t* data_sz)
{
	if (!data || !data_sz || *data_sz <= 0) return 0;
	size_t dst_sz = *data_sz + *data_sz/8 + 16;
	void* dst = lp_alloc(0, dst_sz);

	int32_t i, c, len, r, s, last_match_length, code_buf_ptr;
	uint8_t code_buf[17];
	uint16_t mask;
	uint8_t* filesize;
	uint32_t curmatch;		// PONDER: doesn't this do what r does?
	uint32_t savematch;

	struct LPCodecLZ77 ctx = { 0 };
	ctx.InSize = (uint32_t)*data_sz;
	ctx.InBuf = data;
	ctx.OutBuf = dst;

	ctx.OutSize = 4;  // skip the compression type and file size
	ctx.InOffset = 0;
	ctx.match_position = curmatch = LP_LZ77_RING_MAX - LP_LZ77_FRAME_MAX;

	lp_cod_lz77_inittree(&ctx);  // initialize trees
	code_buf[0] = 0;  /* code_buf[1..16] saves eight units of code, and
					  code_buf[0] works as eight flags, "0" representing that the unit
					  is an unencoded letter (1 byte), "1" a position-and-length pair
					  (2 bytes).  Thus, eight units require at most 16 bytes of code. */
	code_buf_ptr = 1;
	s = 0;  r = LP_LZ77_RING_MAX - LP_LZ77_FRAME_MAX;

	// Clear the buffer
	for (i = s; i < r; i++)
		ctx.text_buf[i] = LP_LZ77_TEXT_BUF_CLEAR;
	// Read LP_LZ77_FRAME_MAX bytes into the last LP_LZ77_FRAME_MAX bytes of the buffer
	for (len = 0; len < LP_LZ77_FRAME_MAX && (c = lp_cod_lz77_inchar(&ctx)) != -1; len++)
		ctx.text_buf[r + len] = c;

	/* Insert the F strings, each of which begins with one or more
	// 'space' characters.  Note the order in which these strings are
	// inserted.  This way, degenerate trees will be less likely to occur.
	*/
	// Perhaps. 
	// However, the strings you create here have no relation to 
	// the actual data and are therefore completely bogus. Removed!
	//for (i = 1; i <= LP_LZ77_FRAME_MAX; i++)
	//	lp_cod_lz77_insertnode(r - i);

	// Create the first node, sets match_length to 0
	lp_cod_lz77_insertnode(&ctx, r);

	// GBA LZSS masks are big-endian
	mask = 0x80;
	do
	{
		if (ctx.match_length > len)
			ctx.match_length = len;

		// match too short: add one unencoded byte
		if (ctx.match_length <= LP_LZ77_THRESHOLD)
		{
			ctx.match_length = 1;
			code_buf[code_buf_ptr++] = ctx.text_buf[r];
		}
		else	// Long enough: add position and length pair.
		{
			code_buf[0] |= mask;	// set match flag

									// 0 byte is 4:length and 4:top 4 bits of match_position
			savematch = ((curmatch - ctx.match_position)&LP_LZ77_NMASK) - 1;
			code_buf[code_buf_ptr++] = ((uint8_t)((savematch >> 8) & 0xf))
				| ((ctx.match_length - (LP_LZ77_THRESHOLD + 1)) << 4);

			code_buf[code_buf_ptr++] = (uint8_t)savematch;
		}
		curmatch += ctx.match_length;
		curmatch &= LP_LZ77_NMASK;

		// if mask is empty, the buffer's full; write it out the code buffer
		// at end of source, code_buf_ptr will be <17
		if ((mask >>= 1) == 0)
		{
			for (i = 0; i < code_buf_ptr; i++)
				ctx.OutBuf[ctx.OutSize++] = code_buf[i];

			ctx.codesize += code_buf_ptr;
			code_buf[0] = 0;
			code_buf_ptr = 1;
			mask = 0x80;
		}

		// Inserts nodes for this match. The last_match_length is 
		// required because lp_cod_lz77_insertnode changes match_length.
		last_match_length = ctx.match_length;
		for (i = 0; i < last_match_length && (c = lp_cod_lz77_inchar(&ctx)) != -1; i++)
		{
			lp_cod_lz77_deletenode(&ctx, s);      // Delete string beforelook-ahead
			ctx.text_buf[s] = c;    // place new bytes
								// text_buf[N..LP_LZ77_RING_MAX+LP_LZ77_FRAME_MAX> is a double for text_buf[0..LP_LZ77_FRAME_MAX>
								// for easier string comparison
			if (s < LP_LZ77_FRAME_MAX - 1)
				ctx.text_buf[s + LP_LZ77_RING_MAX] = c;

			// add and wrap around the buffer
			s = (s + 1) & LP_LZ77_NMASK;
			r = (r + 1) & LP_LZ77_NMASK;

			// Register the string in text_buf[r..r+LP_LZ77_FRAME_MAX-1]
			lp_cod_lz77_insertnode(&ctx, r);
		}

		while (i++ < last_match_length)
		{
			// After the end of text
			lp_cod_lz77_deletenode(&ctx, s);            // no need to read, but
			s = (s + 1) & LP_LZ77_NMASK;
			r = (r + 1) & LP_LZ77_NMASK;
			if (--len)
				lp_cod_lz77_insertnode(&ctx, r);        // buffer may not be empty
		}
	} while (len > 0);    // until length of string to be processed is zero

	if (code_buf_ptr > 1)
	{
		// Send remaining code.
		for (i = 0; i < code_buf_ptr; i++)
			ctx.OutBuf[ctx.OutSize++] = code_buf[i];

		ctx.codesize += code_buf_ptr;
	}

	filesize = (uint8_t*)ctx.OutBuf;
	filesize[0] = LP_CODEC_LZ77;
	filesize[1] = ((ctx.InSize >> 0) & 0xFF);
	filesize[2] = ((ctx.InSize >> 8) & 0xFF);
	filesize[3] = ((ctx.InSize >> 16) & 0xFF);

	*data_sz = (size_t)(uintptr_t)LP_ALIGN(ctx.OutSize, 4);
	dst = lp_alloc(dst, *data_sz);
	return dst;
} 
