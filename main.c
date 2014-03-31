#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <netinet/in.h>
#include <unistd.h>
#include <math.h>

#include "common.h"

int order;
FILE *gifp;

int dng_version = 0;
int zero_after_ff;
ushort white[8][8], curve[0x10000], cr2_slice[3], sraw_mul[4];
ushort *raw_image, (*image)[4], cblack[4102];

int colors = 3;
int flip = 0;

unsigned tiff_nifds, tiff_samples, tiff_bps, tiff_compress, tiff_offset;
ushort raw_height, raw_width, top_margin, left_margin;
ushort iwidth, iheight;
int maximum;

struct tiff_ifd {
	int width, height, bps, comp, phint, offset, flip, samples, bytes;
	int tile_width, tile_length;
} tiff_ifd[10];

#define RAW(row,col) \
	raw_image[(row)*raw_width+(col)]
#define FC(row,col) \
	(filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3)


int parse_tiff (FILE *ifp, int base);

void read_shorts (FILE *ifp, ushort *pixel, int count)
{
	if (fread (pixel, 2, count, ifp) < count)
		printf("read error!\n");
	
	if ((order == 0x4949) == (ntohs(0x1234) == 0x1234))
		swab (pixel, pixel, count*2);
}

unsigned short sget2(unsigned char *s)
{
	if (order == 0x4949)/* "II"  means little-endian */
		return s[0] | s[1] << 8; 
	else			/* "MM" means big-endian */
		return s[0] << 8 | s[1];
}

int get2(FILE *ifp)
{
	unsigned char str[2] = {0xff, 0xff};

	fread(str, 1, 2, ifp);
	return sget2(str);
}
unsigned sget4 (unsigned char *s)
{
	if (order == 0x4949)
		return s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
	else
		return s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
}

unsigned get4(FILE *ifp)
{
	unsigned char str[4] = { 0xff,0xff,0xff,0xff };
	fread (str, 1, 4, ifp);
	return sget4(str);
}

void tiff_get (FILE *ifp, unsigned base, unsigned *tag, unsigned *type, unsigned *len, unsigned *save)
{
	*tag  = get2(ifp);
	*type = get2(ifp);
	*len  = get4(ifp);
	*save = ftell(ifp) + 4;
	if (*len * ("11124811248488"[*type < 14 ? *type:0]-'0') > 4)
		fseek (ifp, get4(ifp)+base, SEEK_SET);
}
unsigned getint (FILE *ifp, int type)
{
	return type == 3 ? get2(ifp) : get4(ifp);
}
/* double CLASS getreal (int type) */
/* { */
/* 	union { char c[8]; double d; } u; */
/* 	int i, rev; */

/* 	switch (type) { */
/* 	case 3: return (unsigned short) get2(); */
/* 	case 4: return (unsigned int) get4(); */
/* 	case 5:  u.d = (unsigned int) get4(); */
/* 		return u.d / (unsigned int) get4(); */
/* 	case 8: return (signed short) get2(); */
/* 	case 9: return (signed int) get4(); */
/* 	case 10: u.d = (signed int) get4(); */
/* 		return u.d / (signed int) get4(); */
/* 	case 11: return int_to_float (get4()); */
/* 	case 12: */
/* 		rev = 7 * ((order == 0x4949) == (ntohs(0x1234) == 0x1234)); */
/* 		for (i=0; i < 8; i++) */
/* 			u.c[i ^ rev] = fgetc(ifp); */
/* 		return u.d; */
/* 	default: return fgetc(ifp); */
/* 	} */
/* } */

void parse_exif (FILE *ifp, int base)
{
	unsigned entries, tag, type, len, save;

	entries = get2(ifp);
	printf("%s:..%d entries\n", __func__, entries);
	while (entries--) {
		tiff_get (ifp, base, &tag, &type, &len, &save);
		switch (tag) {
		case 33434:
//			shutter = getreal(type);
			printf("...%d\n", type);
			break;
		case 33437:
//			aperture = getreal(type);
			printf("...%d\n", type);
			break;
		case 37500:
			printf("...%d\n", type);
//			parse_makernote (base, 0);
			break;
		default:
			break;
		}

		fseek (ifp, save, SEEK_SET);
	}
	
}

struct jhead {
  int bits, high, wide, clrs, sraw, psv, restart, vpred[6];
  ushort *huff[6], *free[4], *row;
};
/*
   Construct a decode tree according the specification in *source.
   The first 16 bytes specify how many codes should be 1-bit, 2-bit
   3-bit, etc.  Bytes after that are the leaf values.

   For example, if the source is

    { 0,1,4,2,3,1,2,0,0,0,0,0,0,0,0,0,
      0x04,0x03,0x05,0x06,0x02,0x07,0x01,0x08,0x09,0x00,0x0a,0x0b,0xff  },

   then the code is

	00		0x04
	010		0x03
	011		0x05
	100		0x06
	101		0x02
	1100		0x07
	1101		0x01
	11100		0x08
	11101		0x09
	11110		0x00
	111110		0x0a
	1111110		0x0b
	1111111		0xff
 */
ushort *make_decoder_ref (const uchar **source)
{
	int max, len, h, i, j;
	const uchar *count;
	ushort *huff;

	count = (*source += 16) - 17;
	for (max=16; max && !count[max]; max--);
	huff = (ushort *) calloc (1 + (1 << max), sizeof *huff);

	huff[0] = max;
	for (h=len=1; len <= max; len++)
		for (i=0; i < count[len]; i++, ++*source)
			for (j=0; j < 1 << (max-len); j++)
				if (h <= 1 << max)
					huff[h++] = len << 8 | **source;
	return huff;
}
ushort * make_decoder (const uchar *source)
{
	return make_decoder_ref (&source);
}

int ljpeg_start (FILE *ifp, struct jhead *jh, int info_only)
{
	int c, tag, len;
	uchar data[0x10000];
	const uchar *dp;

	memset (jh, 0, sizeof *jh);
	jh->restart = INT_MAX;
	fread (data, 2, 1, ifp);
	if (data[1] != 0xd8)
		return 0;
	do {
		fread (data, 2, 2, ifp);
		tag =  data[0] << 8 | data[1];
		len = (data[2] << 8 | data[3]) - 2;
		if (tag <= 0xff00) return 0;
		fread (data, 1, len, ifp);
		switch (tag) {
		case 0xffc3:
			jh->sraw = ((data[7] >> 4) * (data[7] & 15) - 1) & 3;
		case 0xffc0:
			jh->bits = data[0];
			jh->high = data[1] << 8 | data[2];
			jh->wide = data[3] << 8 | data[4];
			jh->clrs = data[5] + jh->sraw;
			if (len == 9 && !dng_version) getc(ifp);
			break;
		case 0xffc4:
			if (info_only) break;
			for (dp = data; dp < data+len && (c = *dp++) < 4; )
				jh->free[c] = jh->huff[c] = make_decoder_ref (&dp);
			break;
		case 0xffda:
			jh->psv = data[1+data[0]*2];
			jh->bits -= data[3+data[0]*2] & 15;
			break;
		case 0xffdd:
			jh->restart = data[0] << 8 | data[1];
		}
	} while (tag != 0xffda);
	if (info_only) return 1;
	if (jh->clrs > 6 || !jh->huff[0]) return 0;
	FORC(5) if (!jh->huff[c+1]) jh->huff[c+1] = jh->huff[c];
	if (jh->sraw) {
		FORC(4)        jh->huff[2+c] = jh->huff[1];
		FORC(jh->sraw) jh->huff[1+c] = jh->huff[0];
	}
	jh->row = (ushort *) calloc (jh->wide*jh->clrs, 4);

	return zero_after_ff = 1;
}


int parse_tiff_ifd(FILE *ifp, int base)
{
	unsigned entries, tag, type, len, save;
	int ifd=0;
	struct jhead jh;
	int i;
	
	ifd = tiff_nifds++;
	
	entries = get2(ifp);
	printf("\n\n\n%s:..%d entries\n", __func__, entries);
	while (entries--){
		tiff_get(ifp, base, &tag, &type, &len, &save);
		printf("tag: %d, type: %d, len: %d\n", tag, type, len);
		switch (tag){
		case 256:
			tiff_ifd[ifd].width = getint(ifp, type);
			break;
		case 257:
			tiff_ifd[ifd].height = getint(ifp, type);
			break;
		case 258:	/* bit per sample */
			tiff_ifd[ifd].samples = len & 7;
			tiff_ifd[ifd].bps = getint(ifp, type);
			break;
		case 259:				/* Compression */
			tiff_ifd[ifd].comp = getint(ifp, type);
			break;
		case 34665:			/* EXIF tag */
			fseek (ifp, get4(ifp)+base, SEEK_SET);
			parse_exif (ifp, base);
			break;
		case 34853:			/* GPSInfo tag */
			/* fseek (ifp, get4(ifp)+base, SEEK_SET); */
			/* parse_gps (base); */
			break;
		case 279:				/* StripByteCounts */
		case 514:
			tiff_ifd[ifd].bytes = get4(ifp);
			break;
		case 273:				/* StripOffset */
			tiff_ifd[ifd].offset = get4(ifp)+base;
			if (!tiff_ifd[ifd].bps && tiff_ifd[ifd].offset > 0) {
				fseek (ifp, tiff_ifd[ifd].offset, SEEK_SET);
				if (ljpeg_start (ifp, &jh, 1)) {
					tiff_ifd[ifd].comp    = 6;
					tiff_ifd[ifd].width   = jh.wide;
					tiff_ifd[ifd].height  = jh.high;
					tiff_ifd[ifd].bps     = jh.bits;
					tiff_ifd[ifd].samples = jh.clrs;
					if (!(jh.sraw || (jh.clrs & 1)))
						tiff_ifd[ifd].width *= jh.clrs;
					i = order;
					parse_tiff (ifp, tiff_ifd[ifd].offset + 12);
					order = i;
				}
			}
			break;

		case 50752:
			read_shorts (ifp, cr2_slice, 3);
			break;
			
		default:
			break;
		}

		
		fseek (ifp, save, SEEK_SET);
	}
	printf("ifd: %d... width: %d, height: %d, samples: %d, bits per sample: %d\n",
	       ifd,
	       tiff_ifd[ifd].width,
	       tiff_ifd[ifd].height,
	       tiff_ifd[ifd].samples,
	       tiff_ifd[ifd].bps);
	printf("ifd: %d...strip offset: %d, StripByteCounts: %d\n",
	       ifd,
	       tiff_ifd[ifd].offset,
	       tiff_ifd[ifd].bytes);

	return 0;
}

int parse_tiff (FILE *ifp, int base)
{
	int doff;

	fseek (ifp, base, SEEK_SET);
	order = get2(ifp);
	if (order != 0x4949 && order != 0x4d4d)
		return 0;
	
	get2(ifp);
	
	tiff_nifds = 0;
	while ((doff = get4(ifp))) {
		fseek (ifp, doff+base, SEEK_SET);
		if (parse_tiff_ifd (ifp, base))
			break;
	}
	
	return 1;
}

void crw_init_tables (unsigned table, ushort *huff[2])
{
	static const uchar first_tree[3][29] = {
		{ 0,1,4,2,3,1,2,0,0,0,0,0,0,0,0,0,
		  0x04,0x03,0x05,0x06,0x02,0x07,0x01,0x08,0x09,0x00,0x0a,0x0b,0xff  },
		{ 0,2,2,3,1,1,1,1,2,0,0,0,0,0,0,0,
		  0x03,0x02,0x04,0x01,0x05,0x00,0x06,0x07,0x09,0x08,0x0a,0x0b,0xff  },
		{ 0,0,6,3,1,1,2,0,0,0,0,0,0,0,0,0,
		  0x06,0x05,0x07,0x04,0x08,0x03,0x09,0x02,0x00,0x0a,0x01,0x0b,0xff  },
	};
	static const uchar second_tree[3][180] = {
		{ 0,2,2,2,1,4,2,1,2,5,1,1,0,0,0,139,
		  0x03,0x04,0x02,0x05,0x01,0x06,0x07,0x08,
		  0x12,0x13,0x11,0x14,0x09,0x15,0x22,0x00,0x21,0x16,0x0a,0xf0,
		  0x23,0x17,0x24,0x31,0x32,0x18,0x19,0x33,0x25,0x41,0x34,0x42,
		  0x35,0x51,0x36,0x37,0x38,0x29,0x79,0x26,0x1a,0x39,0x56,0x57,
		  0x28,0x27,0x52,0x55,0x58,0x43,0x76,0x59,0x77,0x54,0x61,0xf9,
		  0x71,0x78,0x75,0x96,0x97,0x49,0xb7,0x53,0xd7,0x74,0xb6,0x98,
		  0x47,0x48,0x95,0x69,0x99,0x91,0xfa,0xb8,0x68,0xb5,0xb9,0xd6,
		  0xf7,0xd8,0x67,0x46,0x45,0x94,0x89,0xf8,0x81,0xd5,0xf6,0xb4,
		  0x88,0xb1,0x2a,0x44,0x72,0xd9,0x87,0x66,0xd4,0xf5,0x3a,0xa7,
		  0x73,0xa9,0xa8,0x86,0x62,0xc7,0x65,0xc8,0xc9,0xa1,0xf4,0xd1,
		  0xe9,0x5a,0x92,0x85,0xa6,0xe7,0x93,0xe8,0xc1,0xc6,0x7a,0x64,
		  0xe1,0x4a,0x6a,0xe6,0xb3,0xf1,0xd3,0xa5,0x8a,0xb2,0x9a,0xba,
		  0x84,0xa4,0x63,0xe5,0xc5,0xf3,0xd2,0xc4,0x82,0xaa,0xda,0xe4,
		  0xf2,0xca,0x83,0xa3,0xa2,0xc3,0xea,0xc2,0xe2,0xe3,0xff,0xff  },
		{ 0,2,2,1,4,1,4,1,3,3,1,0,0,0,0,140,
		  0x02,0x03,0x01,0x04,0x05,0x12,0x11,0x06,
		  0x13,0x07,0x08,0x14,0x22,0x09,0x21,0x00,0x23,0x15,0x31,0x32,
		  0x0a,0x16,0xf0,0x24,0x33,0x41,0x42,0x19,0x17,0x25,0x18,0x51,
		  0x34,0x43,0x52,0x29,0x35,0x61,0x39,0x71,0x62,0x36,0x53,0x26,
		  0x38,0x1a,0x37,0x81,0x27,0x91,0x79,0x55,0x45,0x28,0x72,0x59,
		  0xa1,0xb1,0x44,0x69,0x54,0x58,0xd1,0xfa,0x57,0xe1,0xf1,0xb9,
		  0x49,0x47,0x63,0x6a,0xf9,0x56,0x46,0xa8,0x2a,0x4a,0x78,0x99,
		  0x3a,0x75,0x74,0x86,0x65,0xc1,0x76,0xb6,0x96,0xd6,0x89,0x85,
		  0xc9,0xf5,0x95,0xb4,0xc7,0xf7,0x8a,0x97,0xb8,0x73,0xb7,0xd8,
		  0xd9,0x87,0xa7,0x7a,0x48,0x82,0x84,0xea,0xf4,0xa6,0xc5,0x5a,
		  0x94,0xa4,0xc6,0x92,0xc3,0x68,0xb5,0xc8,0xe4,0xe5,0xe6,0xe9,
		  0xa2,0xa3,0xe3,0xc2,0x66,0x67,0x93,0xaa,0xd4,0xd5,0xe7,0xf8,
		  0x88,0x9a,0xd7,0x77,0xc4,0x64,0xe2,0x98,0xa5,0xca,0xda,0xe8,
		  0xf3,0xf6,0xa9,0xb2,0xb3,0xf2,0xd2,0x83,0xba,0xd3,0xff,0xff  },
		{ 0,0,6,2,1,3,3,2,5,1,2,2,8,10,0,117,
		  0x04,0x05,0x03,0x06,0x02,0x07,0x01,0x08,
		  0x09,0x12,0x13,0x14,0x11,0x15,0x0a,0x16,0x17,0xf0,0x00,0x22,
		  0x21,0x18,0x23,0x19,0x24,0x32,0x31,0x25,0x33,0x38,0x37,0x34,
		  0x35,0x36,0x39,0x79,0x57,0x58,0x59,0x28,0x56,0x78,0x27,0x41,
		  0x29,0x77,0x26,0x42,0x76,0x99,0x1a,0x55,0x98,0x97,0xf9,0x48,
		  0x54,0x96,0x89,0x47,0xb7,0x49,0xfa,0x75,0x68,0xb6,0x67,0x69,
		  0xb9,0xb8,0xd8,0x52,0xd7,0x88,0xb5,0x74,0x51,0x46,0xd9,0xf8,
		  0x3a,0xd6,0x87,0x45,0x7a,0x95,0xd5,0xf6,0x86,0xb4,0xa9,0x94,
		  0x53,0x2a,0xa8,0x43,0xf5,0xf7,0xd4,0x66,0xa7,0x5a,0x44,0x8a,
		  0xc9,0xe8,0xc8,0xe7,0x9a,0x6a,0x73,0x4a,0x61,0xc7,0xf4,0xc6,
		  0x65,0xe9,0x72,0xe6,0x71,0x91,0x93,0xa6,0xda,0x92,0x85,0x62,
		  0xf3,0xc5,0xb2,0xa4,0x84,0xba,0x64,0xa5,0xb3,0xd2,0x81,0xe5,
		  0xd3,0xaa,0xc4,0xca,0xf2,0xb1,0xe4,0xd1,0x83,0x63,0xea,0xc3,
		  0xe2,0x82,0xf1,0xa3,0xc2,0xa1,0xc1,0xe3,0xa2,0xe1,0xff,0xff  }
	};
	if (table > 2)
		table = 2;
	huff[0] = make_decoder(first_tree[table]);
	huff[1] = make_decoder(second_tree[table]);
}

int canon_has_lowbits(FILE *ifp)
{
	uchar test[0x4000];
	int ret=1, i;

	fseek (ifp, 0, SEEK_SET);
	fread (test, 1, sizeof test, ifp);
	for (i=540; i < sizeof test - 1; i++)
		if (test[i] == 0xff) {
			if (test[i+1]) return 1;
			ret=0;
		}
	return ret;
}

unsigned getbithuff (int nbits, ushort *huff)
{
	static unsigned bitbuf=0;
	static int vbits=0, reset=0;
	unsigned c;
	FILE *ifp;

	ifp = gifp;
	if (nbits > 25)
		return 0;
	if (nbits < 0)
		return bitbuf = vbits = reset = 0;
	if (nbits == 0 || vbits < 0)
		return 0;
	while (!reset && vbits < nbits && (c = fgetc(ifp)) != EOF &&
	       !(reset = zero_after_ff && c == 0xff && fgetc(ifp))) {
		bitbuf = (bitbuf << 8) + (uchar) c;
		vbits += 8;
	}
	c = bitbuf << (32-vbits) >> (32-nbits);
	if (huff) {
		vbits -= huff[c] >> 8;
		c = (uchar) huff[c];
	} else
		vbits -= nbits;
	if (vbits < 0)
		printf("vbits error\n");
	
	return c;
}

#define getbits(n) getbithuff(n,0)
#define gethuff(h) getbithuff(*h,h+1)

void canon_load_raw(FILE *ifp)
{
	ushort *pixel, *prow, *huff[2];
	int nblocks, lowbits, i, c, row, r, save, val;
	
	int block, diffbuf[64], leaf, len, diff, carry=0, pnum=0, base[2];

	crw_init_tables (tiff_compress, huff);
	lowbits = canon_has_lowbits(ifp);
	if (!lowbits) maximum = 0x3ff;
	fseek (ifp, 540 + lowbits*raw_height*raw_width/4, SEEK_SET);
	zero_after_ff = 1;
	getbits(-1);
	for (row=0; row < raw_height; row+=8) {
		pixel = raw_image + row*raw_width;
		nblocks = MIN (8, raw_height-row) * raw_width >> 6;
		for (block=0; block < nblocks; block++) {
			memset (diffbuf, 0, sizeof diffbuf);
			for (i=0; i < 64; i++ ) {
				leaf = gethuff(huff[i > 0]);
				if (leaf == 0 && i) break;
				if (leaf == 0xff) continue;
				i  += leaf >> 4;
				len = leaf & 15;
				if (len == 0) continue;
				diff = getbits(len);
				if ((diff & (1 << (len-1))) == 0)
					diff -= (1 << len) - 1;
				if (i < 64) diffbuf[i] = diff;
			}
			diffbuf[0] += carry;
			carry = diffbuf[0];
			for (i=0; i < 64; i++ ) {
				if (pnum++ % raw_width == 0)
					base[0] = base[1] = 512;
				if ((pixel[(block << 6) + i] = base[i & 1] += diffbuf[i]) >> 10)
					printf("%s:unknow error.\n", __func__);
			}
		}
		if (lowbits) {
			save = ftell(ifp);
			fseek (ifp, 26 + row*raw_width/4, SEEK_SET);
			for (prow=pixel, i=0; i < raw_width*2; i++) {
				c = fgetc(ifp);
				for (r=0; r < 8; r+=2, prow++) {
					val = (*prow << 2) + ((c >> r) & 3);
					if (raw_width == 2672 && val < 512) val += 2;
					*prow = val;
				}
			}
			fseek (ifp, save, SEEK_SET);
		}
	}
	FORC(2) free (huff[c]);
}

int ljpeg_diff (ushort *huff)
{
	int len, diff;

	len = gethuff(huff);
	if (len == 16 && (!dng_version || dng_version >= 0x1010000))
		return -32768;
	diff = getbits(len);
	if ((diff & (1 << (len-1))) == 0)
		diff -= (1 << len) - 1;
	return diff;
}

ushort * ljpeg_row (int jrow, struct jhead *jh)
{
	int col, c, diff, pred, spred=0;
	ushort mark=0, *row[3];

	FILE *ifp = gifp;
	
	if (jrow * jh->wide % jh->restart == 0) {
		FORC(6) jh->vpred[c] = 1 << (jh->bits-1);
		if (jrow) {
			fseek (ifp, -2, SEEK_CUR);
			do mark = (mark << 8) + (c = fgetc(ifp));
			while (c != EOF && mark >> 4 != 0xffd);
		}
		getbits(-1);
	}
	FORC3 row[c] = jh->row + jh->wide*jh->clrs*((jrow+c) & 1);
	for (col=0; col < jh->wide; col++)
		FORC(jh->clrs) {
			diff = ljpeg_diff (jh->huff[c]);
			if (jh->sraw && c <= jh->sraw && (col | c))
				pred = spred;
			else if (col) pred = row[0][-jh->clrs];
			else	    pred = (jh->vpred[c] += diff) - diff;
			if (jrow && col) switch (jh->psv) {
				case 1:	break;
				case 2: pred = row[1][0];
					break;
				case 3: pred = row[1][-jh->clrs];
					break;
				case 4: pred = pred +   row[1][0] - row[1][-jh->clrs];
					break;
				case 5: pred = pred + ((row[1][0] - row[1][-jh->clrs]) >> 1);
					break;
				case 6: pred = row[1][0] + ((pred - row[1][-jh->clrs]) >> 1);
					break;
				case 7: pred = (pred + row[1][0]) >> 1;
					break;
				default: pred = 0;
				}
			if ((**row = pred + diff) >> jh->bits)
				printf("%s: unknow error\n", __func__);
			
			if (c <= jh->sraw) spred = **row;
			row[0]++; row[1]++;
		}
	return row[2];
}
void ljpeg_end (struct jhead *jh)
{
	int c;
	FORC4 if (jh->free[c])
		free (jh->free[c]);
	free (jh->row);
}
int load_flags = 0;

void lossless_jpeg_load_raw(FILE *ifp)
{
	int jwide, jrow, jcol, val, jidx, i, j, row=0, col=0;
	struct jhead jh;
	ushort *rp;

	int height = raw_height;
	
	if (!ljpeg_start (ifp, &jh, 0))
		return;
	jwide = jh.wide * jh.clrs;

	for (jrow=0; jrow < jh.high; jrow++) {
		rp = ljpeg_row (jrow, &jh);
		if (load_flags & 1)
			row = jrow & 1 ? height-1-jrow/2 : jrow/2;
		for (jcol=0; jcol < jwide; jcol++) {
			val = curve[*rp++];
			if (cr2_slice[0]) {
				jidx = jrow*jwide + jcol;
				i = jidx / (cr2_slice[1]*jh.high);
				if ((j = i >= cr2_slice[0]))
					i  = cr2_slice[0];
				jidx -= i * (cr2_slice[1]*jh.high);
				row = jidx / cr2_slice[1+j];
				col = jidx % cr2_slice[1+j] + i*cr2_slice[1];
			}
			if (raw_width == 3984 && (col -= 2) < 0)
				col += (row--,raw_width);
			if ((unsigned) row < raw_height)
				RAW(row,col) = val;
			if (++col >= raw_width)
				col = (row++,0);
		}
	}
	ljpeg_end (&jh);
}

void write2bayer(FILE *ofp, ushort *image, ushort w, ushort h)
{
	int ret;
	
	ret = fwrite(image, sizeof(char), ((h+7)*w*2), ofp);
	if (ret < 0)
		printf("write error %d\n", ret);
	else
		printf("write %d\n", ret);

}


typedef struct 
{
	unsigned short b;
	unsigned short g;
	unsigned short r;
}BGR;

int bayer16torgb24(BGR *rgb24, unsigned short *bayer, int w, int h)
{
	int i,j;
	int px0, px1, px2, px3;

	if ((rgb24 == NULL) || (bayer == NULL))
		return -1;
	if ((w <= 0) || (h <= 0))
		return -1;

	for (j = 0; j < h; j+= 2){
		fprintf(stderr, "%s... %d, %d, %d\n", __FUNCTION__, w, h, j);
		for (i = 0; i < w; i += 2){
			px0 = j * w + i; 
			px1 = px0 + 1; 
			px2 = px0 + w;
			px3 = px0 + w + 1;
			
			if (i == 0 || j == 0 || i == w-2 || j == h - 2){
				rgb24[px0].b = rgb24[px1].b = rgb24[px2].b = rgb24[px3].b = bayer[px2];
				rgb24[px0].g = bayer[px0];
				rgb24[px1].g = rgb24[px2].g = (bayer[px0] + bayer[px3]) / 2;
				rgb24[px3].g = bayer[px3];
				rgb24[px0].r = rgb24[px1].r = rgb24[px2].r = rgb24[px3].r = bayer[px2];
			}
			else {
				rgb24[px0].b = (bayer[px0 - w] + bayer[px0 + w]) / 2;
				rgb24[px0].g = bayer[px0];
				rgb24[px0].r = (bayer[px0 + 1] + bayer[px0 - 1]) / 2;

				rgb24[px1].b = bayer[px1];
				rgb24[px1].g = (bayer[px1 - 1] + bayer[px1 + 1] + bayer[px1 + w] +
				                bayer[px1 - w]) / 4;
				rgb24[px1].r = (bayer[px1 - 1 + w] + bayer[px1 - 1 - w] + bayer[px1 + 1 + w] +
				                bayer[px1 + 1 - w]) / 4;

				rgb24[px2].b = (bayer[px2 - 1 + w] + bayer[px2 - 1 - w] + bayer[px2 + 1 + w] +
				                bayer[px2 + 1 - w]) / 4;
				rgb24[px2].g = (bayer[px2 - 1] + bayer[px2 + 1] + bayer[px2 + w] +
				                bayer[px2 - w]) / 4;
				rgb24[px2].r = bayer[px2];

				rgb24[px3].b = (bayer[px3+w] + bayer[px3-w]) / 2;
				rgb24[px3].g = bayer[px3];
				rgb24[px3].r = (bayer[px3 + 1] + bayer[px3 - 1]) / 2;
			}
		}
	}
	
	return 0;
}
void write2ppm(FILE *ofp, BGR *rgb24, ushort width, ushort height)
{
	uchar *ppm;
	ushort *ppm2;
	int row, col;
	int output_bps = 8;
	
	ppm = (uchar *) calloc (width, colors*output_bps/8);

	fprintf (ofp, "P%d\n%d %d\n%d\n",
		 colors/2+5, width, height, (1 << output_bps)-1);

	for (row=0; row < height; row++) {
		for (col=0; col < width; col++){
			ppm[col*colors+0] = rgb24[width*row + col].r>>8 ;
			ppm[col*colors+1] = rgb24[width*row + col].g>>8;
			ppm[col*colors+2] = rgb24[width*row + col].b>>8;
		}
		fwrite (ppm, colors*output_bps/8, width, ofp);
	}
	
	free (ppm);
}

int main(int argc, char **argv)
{
	FILE *ifp, *ofp;
	int flen;
	int len;
	int nifds;
	int i;
	
	char head[32], *cp;
	char ifname[32], ofname[32];
	BGR *rgb24;

	strcpy(ifname, argv[1]);
		
	if (argc != 2){
		printf("usage: %s filename\n", argv[0]);
		return -1;
	}

	ifp = fopen(ifname, "rb");
	if (!ifp){
		printf("open file %s error!\n", ifname);
		return -1;
	}

	strcpy (ofname, ifname);
	if ((cp = strrchr (ofname, '.'))) *cp = 0;
	strcat (ofname, ".ppm");
	ofp = fopen (ofname, "wb");
	if (!ofp) {
		perror (ofname);
	}
	
	gifp = ifp;
	
	fseek(ifp, 0, SEEK_SET);
	fread(head, 1, 32, ifp);
	fseek(ifp, 0, SEEK_END);
	flen = ftell(ifp);
	printf("raw file: %s, size: %d bytes\n", argv[1], flen);

	fseek(ifp, 0, SEEK_SET);
	order = get2(ifp);
	len = get4(ifp);

	printf("byte order: %x, data lens: %d\n", order, len);

	parse_tiff(ifp, 0);

	nifds = 3;
	raw_width = tiff_ifd[nifds].width;
	raw_height = tiff_ifd[nifds].height;
	tiff_samples = tiff_ifd[nifds].samples;
	tiff_bps = tiff_ifd[nifds].bps;
	tiff_compress = tiff_ifd[nifds].comp;
	tiff_offset = tiff_ifd[nifds].offset;
	/* gamma curve */
	for (i=0; i < 0x10000; i++)
		curve[i] = i;
		
	raw_image = (ushort *) calloc ((raw_height+7), raw_width*2);
	if (!raw_image)
		printf("calloc mem error!\n");
	memset(raw_image, -1, (raw_height+7)*raw_width*2);
	
	fseeko (ifp, tiff_offset, SEEK_SET);
	
	lossless_jpeg_load_raw(ifp);

	iwidth = raw_width;
	iheight = raw_height;

	rgb24 = (BGR *)malloc(raw_width * raw_height * 3 * 2);
	
	bayer16torgb24(rgb24, raw_image, raw_width, raw_height);
	
//	image = (ushort (*)[4]) calloc (iheight, iwidth*sizeof *image);
	
//	write2bayer(ofp, raw_image, raw_width, raw_height);
	
	write2ppm(ofp, rgb24, raw_width, raw_height);
	
	printf("write to file %s\n", ofname);

	free(rgb24);
	free(raw_image);
	
	fclose(ifp);
	fclose(ofp);
	
	return 0;
}


