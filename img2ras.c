/* img2ras
 *	©2020 Yuichiro Nakada
 * */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
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
			int c = (s[n]||s[n+1]||s[n+2] ? 255 : 0);
			BM_PUT(bm, x, y, c);
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

void color_quant(unsigned char *im, int w, int h, int n_colors, char *name, int flag)
{
	int i;
	unsigned char *pix = im;
	node_heap heap = { 0, 0, 0 };

	oct_node root = node_new(0, 0, 0);
	for (i=0; i < w * h; i++, pix += 3) {
		heap_add(&heap, node_insert(root, pix));
	}

	while (heap.n > n_colors + 1) {
		heap_add(&heap, node_fold(pop_heap(&heap)));
	}

	for (i=1; i < heap.n; i++) {
		oct_node got = heap.buf[i];
		double c = got->count;
		got->r = got->r / c + .5;
		got->g = got->g / c + .5;
		got->b = got->b / c + .5;
		printf("%2d | %3lu %3lu %3lu (%d pixels)\n",
		       i, got->r, got->g, got->b, got->count);
	}

	for (i=0, pix = im; i < w * h; i++, pix += 3) {
		color_replace(root, pix);
	}

	// output
	if (flag&1) stbi_write_jpg("posterized.jpg", w, h, 3, im, 0);
	FILE *fp = fopen(name, "w");
	fprintf(fp, "%%!PS-Adobe-3.0 EPSF-3.0\n");
	fprintf(fp, "%%%%BoundingBox: 0 0 %d %d\n", w, h);
	uint8_t *img = malloc(w * h *3*2);
	for (i=1; i < heap.n; i++) {
		memset(img, 0, w * h *3);
		oct_node got = heap.buf[i];
		int n;
		for (n=0, pix = im; n < w * h; n++, pix += 3) {
			if (got->r==pix[0] && got->g==pix[1] && got->b==pix[2]) {
				img[n*3] = got->r;
				img[n*3+1] = got->g;
				img[n*3+2] = got->b;
				/*img[n*3] = 255;
				img[n*3+1] = 255;
				img[n*3+2] = 255;*/
			}
		}
		if (flag&1) { // debug
			char str[256];
			snprintf(str, sizeof(str), "original_d%02d.bmp", i);
			stbi_write_bmp(str, w, h, 3, img);
		}
		if (flag&4) { // alpha
			if (got->r==255 && got->g==255 && got->b==255) continue;
		}
		if (flag&2) { // dilate
			imgp_dilate(img, w, h, 3, img+w * h *3);
			img2ras(fp, img+w * h *3, w, h, got->r, got->g, got->b);
		} else {
			img2ras(fp, img, w, h, got->r, got->g, got->b);
		}
	}
	free(img);
	fprintf(fp, "%%EOF\n");
	fclose(fp);

	node_free();
	free(heap.buf);
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

void usage(FILE* fp, char** argv)
{
	fprintf(fp,
		"Usage: %s [options] file\n\n"
		"Options:\n"
		"-h                 Print this message\n"
		"-o <output name>   Output file name\n"
		"-c                 Reduce color [default:32]\n"
		"-b <scale>         Blur image\n"
		"\n",
		argv[0]);
}

int main(int argc, char* argv[])
{
	char *name = argv[1];
	char *outfile = "output.eps";
	int color = 32;
	int flag = 0;
	float scale = 2;

	if (argc <=1) {
		usage(stderr, argv);
		return 0;
	}
	for (int i=1; i<argc; i++) {
		if (!strcmp(argv[i], "-o")) {
			outfile = argv[++i];
		} else if (!strcmp(argv[i], "-c")) {
			color = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-d")) {
			flag = 1; // debug
		} else if (!strcmp(argv[i], "-x")) {
			flag = 2; // dilate
		} else if (!strcmp(argv[i], "-a")) {
			flag = 4; // alpha
		} else if (!strcmp(argv[i], "-b")) {
			flag = 8; // blur
			scale = atof(argv[++i]);
		} else if (!strcmp(argv[i], "-h")) {
			usage(stderr, argv);
			return 0;
		} else {
			name = argv[i];
			//printf("%s\n", name);
		}
	}


	uint8_t *pixels;
	int w, h, bpp;
	pixels = stbi_load(name, &w, &h, &bpp, 3);
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
	if (flag&8) { // blur
		int sx = w*scale;	// 2000
		int sy = h*scale;	// 2800
		uint8_t *posterized = malloc(sx*sy *3 *2);
		stbir_resize_uint8_srgb(pixels, w, h, 0, posterized+sx*sy*3, sx, sy, 0, 3, -1, 0);
		imgp_filter(posterized, posterized+sx*sy*3, sx, sy, magic_kernel, 4, 1, 0);
		stbir_resize_uint8_srgb(posterized, sx, sy, 0, /*posterized+sx*sy*3*/pixels, w, h, 0, 3, -1, 0);
		if (flag&1) stbi_write_jpg("magic.jpg", w, h, 3, /*posterized+sx*sy*3*/pixels, 0);
		free(posterized);
	}

	uint8_t *gray = malloc(w*h*4);
	uint8_t *dilated = gray+w*h;
	uint8_t *diff = gray+w*h*2;
	uint8_t *contour = gray+w*h*3;
	imgp_gray(pixels, w, h, w, gray, w);
	color_quant(pixels, w, h, color, outfile, flag);
//	stbi_write_bmp("original64.bmp", w, h, 3, pixels);
//	stbi_write_bmp("gray.bmp", w, h, 1, gray);
	imgp_dilate(gray, w, h, 1, dilated);
//	stbi_write_jpg("dilated.jpg", w, h, 1, dilated, 0);
	imgp_absdiff(gray, dilated, w, h, diff);
//	stbi_write_jpg("diff.jpg", w, h, 1, diff, 0);
	imgp_reverse(diff, w, h, contour);
//	stbi_write_jpg("contour.jpg", w, h, 1, contour, 0);
//	stbi_write_bmp("contour.bmp", w, h, 1, contour);
	free(gray);
	stbi_image_free(pixels);
}
