/*
 *  fixpng.c
 *
 *  Copyright 2007 MHW. Read the GPL v2.0 for legal details.
 *  Copyright 2012 Bastien Nocera <hadess@hadess.net>
 *  http://www.gnu.org/licenses/gpl-2.0.txt
 *
 *
 *  This tool will convert iPhone PNGs from its weird, non-compatible format to
 *  A format any PNG-compatible application will read. It will not flip the R
 *  and B channels.
 *
 *  In summary, this tool takes an input png uncompresses the IDAT chunk, recompresses
 *  it in a PNG-compatible way and then writes everything except the, so far,
 *  useless CgBI chunk to the output.
 *
 *  It's a relatively quick hack, and it will break if the IDAT in either form
 *  (compressed or uncompressed) is larger than 1MB, and if there are more than 20
 *  chunks before the IDAT(s). In that case, poke at MAX_CHUNKS and BUFSIZE.
 *
 *  Usage therefore: fixpng <input.png> <output.png>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>

#include <zlib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define MAX_CHUNKS 20
#define BUFSIZE 1048576 // 1MB buffer size

typedef struct png_chunk_t {
	guint32 length;
	unsigned char *name;
	unsigned char *data;
	guint32 crc;
} png_chunk;

unsigned char pngheader[8] = {137, 80, 78, 71, 13, 10, 26, 10};
unsigned char datachunk[4] = {0x49, 0x44, 0x41, 0x54}; // IDAT
unsigned char endchunk[4] = {0x49, 0x45, 0x4e, 0x44}; // IEND
unsigned char cgbichunk[4] = {0x43, 0x67, 0x42, 0x49}; // CgBI

static void die(char *why);
static int check_png_header(char *);
static GList *read_chunks (char* buf);
static void process_chunks(GList *chunks);
static GdkPixbuf *write_png(GList *chunks, guint idat_idx);
static unsigned long mycrc(unsigned char *, unsigned char *, int);

static guint
get_num_idat (GList *chunks)
{
	GList *l;
	guint num_idats = 0;

	for (l = chunks; l != NULL; l = l->next) {
		png_chunk *chunk = l->data;
		if (memcmp(chunk->name, datachunk, 4) == 0)
			num_idats++;
	}
	return num_idats;
}

static void
chunk_free (png_chunk *chunk)
{
	g_free (chunk->data);
	g_free (chunk);
}

int
main (int argc, char **argv)
{
	char *buf;
	GList *chunks, *pixbufs;
	GError *error = NULL;
	guint num_idat, i;

	g_type_init ();

	if (argc!=3) {
		printf("Usage: %s <input> <output>\n\n",argv[0]);
		exit(1);
	}

	if (g_file_get_contents (argv[1], &buf, NULL, &error) == FALSE) {
		g_warning ("Couldn't read file '%s': %s", argv[1], error->message);
		g_error_free (error);
		return 1;
	}

	if (!check_png_header(buf)){
		g_free (buf);
		die("This is not a PNG file. I require a PNG file!\n");
	}

	chunks = read_chunks(buf);
	g_free (buf);
	process_chunks(chunks);

	num_idat = get_num_idat (chunks);
	pixbufs = NULL;
	for (i = 0; i < num_idat; i++) {
		GdkPixbuf *pixbuf;

		pixbuf = write_png(chunks, i);
		pixbufs = g_list_prepend (pixbufs, pixbuf);
	}
	pixbufs = g_list_reverse (pixbufs);

	/* And discard all the chunks */
	g_list_free_full (chunks, (GDestroyNotify) chunk_free);

	if (num_idat == 2) {
		GdkPixbuf *final;
		GdkPixbuf *first = pixbufs->data;
		GdkPixbuf *second = pixbufs->next->data;

		final = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
					TRUE,
					8,
					gdk_pixbuf_get_width (first),
					gdk_pixbuf_get_height (first));
		gdk_pixbuf_copy_area (first,
				      0, 0,
				      gdk_pixbuf_get_width (first),
				      gdk_pixbuf_get_height (first) / 2,
				      final,
				      0, 0);
		gdk_pixbuf_copy_area (second,
				      0, 0,
				      gdk_pixbuf_get_width (first),
				      gdk_pixbuf_get_height (first) / 2,
				      final,
				      0, gdk_pixbuf_get_height (first) / 2);

		gdk_pixbuf_save (final, argv[2], "png", NULL, NULL);
		g_object_unref (final);
	} else {
		GdkPixbuf *pixbuf = pixbufs->data;

		gdk_pixbuf_save (pixbuf, argv[2], "png", NULL, NULL);
	}

#if 0
	for (l = pixbufs, i = 0; l != NULL; l = l->next, i++) {
		GdkPixbuf *pixbuf = l->data;
		char *filename;

		filename = g_strdup_printf ("test%d.png", i);
		gdk_pixbuf_save (pixbuf, filename, "png", NULL, NULL);
		g_free (filename);
	}
#endif

	g_list_free_full (pixbufs, (GDestroyNotify) g_object_unref);

	return 0;
}

static int
check_png_header(char *buf)
{
	return (!(int) memcmp(buf, pngheader, 8));
}

static GList *
read_chunks (char* buf)
{
	GList *chunks = NULL;
	int i = 0;

	buf += 8;
	do {
		png_chunk *chunk;

		chunk = g_new0 (png_chunk, 1);

		memcpy(&chunk->length, buf, 4);
		chunk->length = ntohl(chunk->length);
		chunk->data = (unsigned char *)malloc(chunk->length);
		chunk->name = (unsigned char *)malloc(4);

		buf += 4;
		memcpy(chunk->name, buf, 4);
		buf += 4;
		chunk->data = (unsigned char *)malloc(chunk->length);
		memcpy(chunk->data, buf, chunk->length);
		buf += chunk->length;
		memcpy(&chunk->crc, buf, 4);
		chunk->crc = ntohl(chunk->crc);
		buf += 4;

#if 0
		printf("Found chunk: %c%c%c%c\n", chunk->name[0], chunk->name[1], chunk->name[2], chunk->name[3]);
		printf("Length: %d, CRC32: %08x\n", chunk->length, chunk->crc);
#endif

		chunks = g_list_prepend (chunks, chunk);

		if (!memcmp(chunk->name, endchunk, 4)){
			// End of img.
			break;
		}
	} while (i++ < MAX_CHUNKS);

	return g_list_reverse (chunks);
}

static void
process_chunks (GList *chunks)
{
	GList *l;

	// Poke at any IDAT chunks and de/recompress them
	for (l = chunks; l != NULL; l = l->next) {
		png_chunk *chunk;
		int ret;
		z_stream infstrm, defstrm;
		unsigned char *inflatedbuf;
		unsigned char *deflatedbuf;

		chunk = l->data;

		/* End chunk */
		if (memcmp(chunk->name, endchunk, 4) == 0)
			break;

		/* Not IDAT */
		if (memcmp(chunk->name, datachunk, 4) != 0)
			continue;

		inflatedbuf = (unsigned char *)g_malloc(BUFSIZE);
		infstrm.zalloc = Z_NULL;
		infstrm.zfree = Z_NULL;
		infstrm.opaque = Z_NULL;
		infstrm.avail_in = chunk->length;
		infstrm.next_in = chunk->data;
		infstrm.next_out = inflatedbuf;
		infstrm.avail_out = BUFSIZE;

		// Inflate using raw inflation
		if (inflateInit2(&infstrm,-8) != Z_OK){
			die("ZLib error");
		}

		ret = inflate(&infstrm, Z_NO_FLUSH);
		switch (ret) {
		case Z_NEED_DICT:
			ret = Z_DATA_ERROR;     /* and fall through */
		case Z_DATA_ERROR:
		case Z_MEM_ERROR:
			printf("ZLib error! %d\n", ret);
			inflateEnd(&infstrm);
		}

		inflateEnd(&infstrm);

		// Now deflate again, the regular, PNG-compatible, way
		deflatedbuf = (unsigned char *)g_malloc(BUFSIZE);

		defstrm.zalloc = Z_NULL;
		defstrm.zfree = Z_NULL;
		defstrm.opaque = Z_NULL;
		defstrm.avail_in = infstrm.total_out;
		defstrm.next_in = inflatedbuf;
		defstrm.next_out = deflatedbuf;
		defstrm.avail_out = BUFSIZE;

		deflateInit(&defstrm, Z_DEFAULT_COMPRESSION);
		deflate(&defstrm, Z_FINISH);

		chunk->data = deflatedbuf;
		chunk->length = defstrm.total_out;
		chunk->crc = mycrc(chunk->name, chunk->data, chunk->length);

		g_free (inflatedbuf);
#if 0
		printf("Chunk: %c%c%c%c, new length: %d, new CRC: %08x\n",
		       chunk->name[0], chunk->name[1],
		       chunk->name[2], chunk->name[3],
		       chunk->length, chunk->crc);
#endif
	}
}

static void
fix_channels (GdkPixbuf *pixbuf)
{
	int img_width = gdk_pixbuf_get_width(pixbuf);
	int img_height = gdk_pixbuf_get_height(pixbuf);
	int row_stride = gdk_pixbuf_get_rowstride(pixbuf);
	int pix_stride = 4;
	guint8 *buf = gdk_pixbuf_get_pixels(pixbuf);
	int col_idx, row_idx;

	for (row_idx = 0; row_idx < img_height; row_idx++) {
		guint8 *row = buf + row_idx * row_stride;
		for (col_idx = 0; col_idx < img_width; col_idx++) {
			guchar *p;
			guchar r, b;

			p = row + col_idx * pix_stride;

			/* Swap R and B */
			r = *p;
			b = *(p + 2);
			*(p + 2) = r;
			*p  = b;

			//FIXME fixup premultiplied alpha
		}
	}
}

static GdkPixbuf *
write_png(GList *chunks, guint idat_idx)
{
	GList *l;
	GByteArray *data;
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf;
	guint idat_seen = 0;

	data = g_byte_array_new ();
	g_byte_array_append (data, pngheader, 8);

	for (l = chunks; l != NULL; l = l->next) {
		png_chunk *chunk;
		int tmp;
		guint crc;

		chunk = l->data;

		tmp = htonl(chunk->length);
		crc = htonl(chunk->crc);

		if (memcmp(chunk->name, cgbichunk, 4)){ // Anything but a CgBI
			if (memcmp(chunk->name, "IDAT", 4) == 0) {
				idat_seen++;

				if (idat_seen - 1 != idat_idx) {
					g_message ("ignoring IDAT %d", idat_seen - 1);
					continue;
				}

				g_message ("adding idat %d", idat_seen - 1);
			}

			g_byte_array_append (data, (const guint8 *) &tmp, 4);
			g_byte_array_append (data, chunk->name, 4);

			if (chunk->length > 0)
				g_byte_array_append (data, chunk->data, chunk->length);

			g_byte_array_append (data, (const guint8 *) &crc, 4);
		}

		if (!memcmp(chunk->name, endchunk, 4)){
			break;
		}
	}

#if 0
	char *filename = g_strdup_printf ("raw-test%d.png", idat_idx);
	g_file_set_contents (filename, data->data, data->len, NULL);
	g_message ("wrote file %s", filename);
	g_free (filename);
#endif

	//FIXME add some debug here
	loader = gdk_pixbuf_loader_new_with_type ("png", NULL);
	gdk_pixbuf_loader_write (loader, data->data, data->len, NULL);
	gdk_pixbuf_loader_close (loader, NULL);
	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	g_object_ref (pixbuf);
	g_object_unref (loader);
	g_byte_array_free (data, FALSE);

	/* Reverse channels */
	fix_channels (pixbuf);

	return pixbuf;
}

static void
die (char *why)
{
	printf(why);
	exit(1);
}

static unsigned long
mycrc (unsigned char *name, unsigned char *buf, int len)
{
	guint32 crc;

	crc = crc32(0, name, 4);
	return crc32(crc, buf, len);
}
