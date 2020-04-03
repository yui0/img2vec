/* img2ras
 *	©2020 Yuichiro Nakada
 * */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "imgp.h"

#include "potracelib.h"
int img2ras(FILE *fp, uint8_t *s, int w, int h, int r, int g, int b)
{
	// create a bitmap
	potrace_bitmap_t *bm = bm_new(w, h);
	if (!bm) {
		fprintf(stderr, "Error allocating bitmap: %s\n", strerror(errno));
		return 1;
	}

	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			int n = ((h-y)*w+x)*3;
			BM_PUT(bm, x, y, (s[n]||s[n+1]||s[n+2]?255:0));
		}
	}

	// set tracing parameters, starting from defaults
	potrace_param_t *param = potrace_param_default();
	if (!param) {
		fprintf(stderr, "Error allocating parameters: %s\n", strerror(errno));
		return 1;
	}
	param->turdsize = 0;

	// trace the bitmap
	potrace_state_t *st = potrace_trace(param, bm);
	if (!st || st->status != POTRACE_STATUS_OK) {
		fprintf(stderr, "Error tracing bitmap: %s\n", strerror(errno));
		return 1;
	}
	bm_free(bm);

	// output vector data, e.g. as a rudimentary EPS file
//	printf("%%!PS-Adobe-3.0 EPSF-3.0\n");
//	printf("%%%%BoundingBox: 0 0 %d %d\n", w, h);
	fprintf(fp, "gsave\n");
	potrace_path_t *p = st->plist;
	potrace_dpoint_t (*c)[3];
	while (p != NULL) {
		int n = p->curve.n;
		int *tag = p->curve.tag;
		c = p->curve.c;
		fprintf(fp, "%f %f moveto\n", c[n-1][2].x, c[n-1][2].y);
		for (int i=0; i<n; i++) {
			switch (tag[i]) {
			case POTRACE_CORNER:
				fprintf(fp, "%f %f lineto\n", c[i][1].x, c[i][1].y);
				fprintf(fp, "%f %f lineto\n", c[i][2].x, c[i][2].y);
				break;
			case POTRACE_CURVETO:
				fprintf(fp, "%f %f %f %f %f %f curveto\n",
				       c[i][0].x, c[i][0].y,
				       c[i][1].x, c[i][1].y,
				       c[i][2].x, c[i][2].y);
				break;
			}
		}
		/* at the end of a group of a positive path and its negative
		   children, fill. */
		if (p->next == NULL || p->next->sign == '+') {
			//printf("0 setgray fill\n");
			fprintf(fp, "%f %f %f setrgbcolor fill\n", r/255.0, g/255.0, b/255.0);
		}
		p = p->next;
	}
	fprintf(fp, "grestore\n");
	//printf("%%EOF\n");

	return 0;
}

// https://www.petitmonte.com/math_algorithm/subtractive_color.html
// https://github.com/kornelski/mediancut-posterizer/blob/master/posterize.c
// https://rosettacode.org/wiki/Color_quantization/C
typedef struct {
	int w, h;
	unsigned char *pix;
} image_t, *image;

typedef struct oct_node_t oct_node_t, *oct_node;
struct oct_node_t {
	/* sum of all colors represented by this node. 64 bit in case of HUGE image */
	uint64_t r, g, b;
	int count, heap_idx;
	oct_node kids[8], parent;
	unsigned char n_kids, kid_idx, flags, depth;
};

typedef struct {
	int alloc, n;
	oct_node* buf;
} node_heap;

/* cmp function that decides the ordering in the heap.  This is how we determine
   which octree node to fold next, the heart of the algorithm. */
int cmp_node(oct_node a, oct_node b)
{
	if (a->n_kids < b->n_kids) return -1;
	if (a->n_kids > b->n_kids) return 1;

	int ac = a->count * (1 + a->kid_idx) >> a->depth;
	int bc = b->count * (1 + b->kid_idx) >> b->depth;
	return ac < bc ? -1 : ac > bc;
}

void down_heap(node_heap *h, oct_node p)
{
	int n = p->heap_idx, m;
	while (1) {
		m = n * 2;
		if (m >= h->n) break;
		if (m + 1 < h->n && cmp_node(h->buf[m], h->buf[m + 1]) > 0) m++;

		if (cmp_node(p, h->buf[m]) <= 0) break;

		h->buf[n] = h->buf[m];
		h->buf[n]->heap_idx = n;
		n = m;
	}
	h->buf[n] = p;
	p->heap_idx = n;
}

void up_heap(node_heap *h, oct_node p)
{
	int n = p->heap_idx;
	oct_node prev;

	while (n>1) {
		prev = h->buf[n / 2];
		if (cmp_node(p, prev) >= 0) break;

		h->buf[n] = prev;
		prev->heap_idx = n;
		n /= 2;
	}
	h->buf[n] = p;
	p->heap_idx = n;
}

#define ON_INHEAP	1
void heap_add(node_heap *h, oct_node p)
{
	if ((p->flags & ON_INHEAP)) {
		down_heap(h, p);
		up_heap(h, p);
		return;
	}

	p->flags |= ON_INHEAP;
	if (!h->n) h->n = 1;
	if (h->n >= h->alloc) {
		while (h->n >= h->alloc) h->alloc += 1024;
		h->buf = realloc(h->buf, sizeof(oct_node) * h->alloc);
	}

	p->heap_idx = h->n;
	h->buf[h->n++] = p;
	up_heap(h, p);
}

oct_node pop_heap(node_heap *h)
{
	if (h->n <= 1) return 0;

	oct_node ret = h->buf[1];
	h->buf[1] = h->buf[--h->n];

	h->buf[h->n] = 0;

	h->buf[1]->heap_idx = 1;
	down_heap(h, h->buf[1]);

	return ret;
}

static oct_node pool = 0;
oct_node node_new(unsigned char idx, unsigned char depth, oct_node p)
{
	static int len = 0;
	if (len <= 1) {
		oct_node p = calloc(sizeof(oct_node_t), 2048);
		p->parent = pool;
		pool = p;
		len = 2047;
	}

	oct_node x = pool + len--;
	x->kid_idx = idx;
	x->depth = depth;
	x->parent = p;
	if (p) p->n_kids++;
	return x;
}

void node_free()
{
	oct_node p;
	while (pool) {
		p = pool->parent;
		free(pool);
		pool = p;
	}
}

/* adding a color triple to octree */
#define OCT_DEPTH 8
/* 8: number of significant bits used for tree.  It's probably good enough
   for most images to use a value of 5.  This affects how many nodes eventually
   end up in the tree and heap, thus smaller values helps with both speed
   and memory. */
oct_node node_insert(oct_node root, unsigned char *pix)
{
	unsigned char i, bit, depth = 0;
	for (bit = 1 << 7; ++depth < OCT_DEPTH; bit >>= 1) {
		i = !!(pix[1] & bit) * 4 + !!(pix[0] & bit) * 2 + !!(pix[2] & bit);
		if (!root->kids[i]) {
			root->kids[i] = node_new(i, depth, root);
		}

		root = root->kids[i];
	}

	root->r += pix[0];
	root->g += pix[1];
	root->b += pix[2];
	root->count++;
	return root;
}

/* remove a node in octree and add its count and colors to parent node. */
oct_node node_fold(oct_node p)
{
	if (p->n_kids) abort();
	oct_node q = p->parent;
	q->count += p->count;

	q->r += p->r;
	q->g += p->g;
	q->b += p->b;
	q->n_kids --;
	q->kids[p->kid_idx] = 0;
	return q;
}

/* traverse the octree just like construction, but this time we replace the pixel
   color with color stored in the tree node */
void color_replace(oct_node root, unsigned char *pix)
{
	unsigned char i, bit;

	for (bit = 1 << 7; bit; bit >>= 1) {
		i = !!(pix[1] & bit) * 4 + !!(pix[0] & bit) * 2 + !!(pix[2] & bit);
		if (!root->kids[i]) break;
		root = root->kids[i];
	}

	pix[0] = root->r;
	pix[1] = root->g;
	pix[2] = root->b;
}

/* Building an octree and keep leaf nodes in a bin heap.  Afterwards remove first node
   in heap and fold it into its parent node (which may now be added to heap), until heap
   contains required number of colors. */
void color_quant(image im, int n_colors, char *name)
{
	int i;
	unsigned char *pix = im->pix;
	node_heap heap = { 0, 0, 0 };

	oct_node root = node_new(0, 0, 0), got;
	for (i=0; i < im->w * im->h; i++, pix += 3) {
		heap_add(&heap, node_insert(root, pix));
	}

	while (heap.n > n_colors + 1) {
		heap_add(&heap, node_fold(pop_heap(&heap)));
	}

	double c;
	for (i=1; i < heap.n; i++) {
		got = heap.buf[i];
		c = got->count;
		got->r = got->r / c + .5;
		got->g = got->g / c + .5;
		got->b = got->b / c + .5;
		printf("%2d | %3lu %3lu %3lu (%d pixels)\n",
		       i, got->r, got->g, got->b, got->count);
	}

	for (i=0, pix = im->pix; i < im->w * im->h; i++, pix += 3) {
		color_replace(root, pix);
	}

	// output
	FILE *fp = fopen(name, "w");
	fprintf(fp, "%%!PS-Adobe-3.0 EPSF-3.0\n");
	fprintf(fp, "%%%%BoundingBox: 0 0 %d %d\n", im->w, im->h);
	uint8_t *img = malloc(im->w * im->h *3);
	for (i=1; i < heap.n; i++) {
		memset(img, 0, im->w * im->h *3);
		got = heap.buf[i];
		int n;
		for (n=0, pix = im->pix; n < im->w * im->h; n++, pix += 3) {
			if (got->r==pix[0] && got->g==pix[1] && got->b==pix[2]) {
				img[n*3] = got->r;
				img[n*3+1] = got->g;
				img[n*3+2] = got->b;
			}
		}
//		char str[256];
//		snprintf(str, sizeof(str), "original_d%02d.bmp", i);
//		stbi_write_bmp(str, im->w, im->h, 3, img);
		img2ras(fp, img, im->w, im->h, got->r, got->g, got->b);
	}
	free(img);
	fprintf(fp, "%%EOF\n");
	fclose(fp);

	node_free();
	free(heap.buf);
}

void _imgp_dilate(uint8_t *s, int w, int h, int bpp, uint8_t *p)
{
	for (int y=1; y<h-1; y++) {
		for (int x=1; x<w-1; x++) {
			for (int b=0; b<bpp; b++) {
				uint8_t uc = s[ (y+0)*w*bpp + (x+0)*bpp+b ]; // centre
				uint8_t ua = s[ (y-1)*w*bpp + (x+0)*bpp+b ]; // above
				uint8_t ub = s[ (y+1)*w*bpp + (x+0)*bpp+b ]; // below
				uint8_t ul = s[ (y+0)*w*bpp + (x-1)*bpp+b ]; // left
				uint8_t ur = s[ (y+0)*w*bpp + (x+1)*bpp+b ]; // right

				uint8_t ux = 0;
				if (uc > ux) ux = uc;
				if (ua > ux) ux = ua;
				if (ub > ux) ux = ub;
				if (ul > ux) ux = ul;
				if (ur > ux) ux = ur;
				p[ y*w*bpp + x*bpp+b ] = ux;
			}
		}
	}
}

/*double emboss_kernel[3*3] = {
  -2., -1.,  0.,
  -1.,  1.,  1.,
  0.,  1.,  2.,
};
double sharpen_kernel[3*3] = {
  -1.0, -1.0, -1.0,
  -1.0,  9.0, -1.0,
  -1.0, -1.0, -1.0
};
double sobel_emboss_kernel[3*3] = { offset+0.5
  -1., -2., -1.,
  0.,  0.,  0.,
  1.,  2.,  1.,
};
double box_blur_kernel[3*3] = {
  1.0/9, 1.0/9, 1.0/9,
  1.0/9, 1.0/9, 1.0/9,
  1.0/9, 1.0/9, 1.0/9,
};*/
// https://postd.cc/the-magic-kernel/
double magic_kernel[4*4] = {
	1/64.0, 3/64.0, 3/64.0, 1/64.0,
	3/64.0, 9/64.0, 9/64.0, 3/64.0,
	3/64.0, 9/64.0, 9/64.0, 3/64.0,
	1/64.0, 3/64.0, 3/64.0, 1/64.0,
};

int main(int argc, char* argv[])
{
	char *name = argv[1];
	char *outfile = "output.eps";
	int color = 32;

	if (argc <=1) {
		return 0;
	}
	for (int i=1; i<argc; i++) {
		if (!strcmp(argv[i], "-o")) {
			outfile = argv[++i];
		} else if (!strcmp(argv[i], "-c")) {
			color = atoi(argv[++i]);
		} else {
			name = argv[i];
			//printf("%s\n", name);
		}
	}


	uint8_t *pixels;
	int w, h, bpp;
	pixels = stbi_load(name, &w, &h, &bpp, 3);
//	stbi_write_bmp("original.bmp", w, h, 3, pixels);
	assert(pixels);

	/*uint8_t *posterized = malloc(w*h*6);
	_imgp_dilate(pixels, w, h, 3, posterized);
	stbi_write_jpg("original_dilated.jpg", w, h, 3, posterized, 0);
	color_quant(&(image_t){w, h, posterized}, 64);
	stbi_write_bmp("original_d64.bmp", w, h, 3, posterized);
	free(posterized);*/
	/*uint8_t *posterized = malloc(w*h*6);
	memcpy(posterized, pixels, w*h*3);
	color_quant(&(image_t){w, h, posterized}, 64);
	stbi_write_bmp("original_d64.bmp", w, h, 3, posterized);
	_imgp_dilate(posterized, w, h, 3, posterized+w*h*3);
	stbi_write_jpg("original_dilated.jpg", w, h, 3, posterized+w*h*3, 0);
	free(posterized);*/
/*	uint8_t *posterized = malloc(w*h*3*2);
	imgp_filter(posterized, pixels, w, h, magic_kernel, 4, 1, 0);
	stbi_write_jpg("magic.jpg", w, h, 3, posterized, 0);
	color_quant(&(image_t) {w, h, posterized}, 64);
	stbi_write_bmp("original_m64.bmp", w, h, 3, posterized);
	free(posterized);*/

	uint8_t *gray = malloc(w*h*4);
	uint8_t *dilated = gray+w*h;
	uint8_t *diff = gray+w*h*2;
	uint8_t *contour = gray+w*h*3;
//	imgp_gray(pixels, w, h, w, gray, w);
	color_quant(&(image_t) {w, h, pixels}, color, outfile);
//	stbi_write_bmp("original64.bmp", w, h, 3, pixels);
//	stbi_write_bmp("gray.bmp", w, h, 1, gray);
	imgp_dilate(gray, w, h/*, 5*/, dilated);
//	stbi_write_jpg("dilated.jpg", w, h, 1, dilated, 0);
	imgp_absdiff(gray, dilated, w, h, diff);
//	stbi_write_jpg("diff.jpg", w, h, 1, diff, 0);
	imgp_reverse(diff, w, h, contour);
//	stbi_write_jpg("contour.jpg", w, h, 1, contour, 0);
//	stbi_write_bmp("contour.bmp", w, h, 1, contour);
	free(gray);
	stbi_image_free(pixels);
}
