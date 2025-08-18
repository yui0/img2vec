/* img2vec: Transforming bitmaps into vector graphics
 * ©2020,2025 Yuichiro Nakada
 */

/*
emcc img2vec.c -o img2vec.js \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_FUNCTIONS="['_main', '_process_image', '_malloc', '_free']" \
  -s EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap', 'FS', 'HEAPU8']" \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s ENVIRONMENT=web \
  -fopenmp \
  -Os \
  -s ASSERTIONS=0 \
  -s VERBOSE=1 \
  -I .
*/

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#include "imgp.h"

#define ACCURACY "%.2f"

#include "potracelib.h"
int img2vec(FILE *fp, uint8_t *s, int w, int h, int r, int g, int b, int flag, int turdsize, double alphamax, double opttolerance)
{
    potrace_bitmap_t *bm = bm_new(w, h);
    if (!bm) {
        fprintf(stderr, "Error allocating bitmap: %s\n", strerror(errno));
        return 1;
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int n = ((h - y - 1) * w + x) * 3; // Y座標反転
            int c = (s[n] == r && s[n + 1] == g && s[n + 2] == b) ? 255 : 0;
            BM_PUT(bm, x, y, c);
        }
    }

    potrace_param_t *param = potrace_param_default();
    if (!param) {
        fprintf(stderr, "Error allocating parameters: %s\n", strerror(errno));
        bm_free(bm);
        return 1;
    }
    param->turdsize = turdsize;
    param->alphamax = alphamax;
    param->opttolerance = opttolerance;

    potrace_state_t *st = potrace_trace(param, bm);
    if (!st || st->status != POTRACE_STATUS_OK) {
        fprintf(stderr, "Error tracing bitmap\n");
        bm_free(bm);
        potrace_param_free(param);
        return 1;
    }
    bm_free(bm);

    potrace_path_t *p = st->plist;
    if (flag & 32) { // SVG
        fprintf(fp, "<g id=\"%02x%02x%02x\">\n", r, g, b);
        fprintf(fp, "<path fill=\"#%02x%02x%02x\" fill-rule=\"evenodd\" d=\"", r, g, b);

        while (p != NULL) {
            int n = p->curve.n;
            potrace_dpoint_t (*c)[3] = p->curve.c;
            int *tag = p->curve.tag;

            if (n == 0) {
                p = p->next;
                continue;
            }

            fprintf(fp, "M" ACCURACY "," ACCURACY, c[n - 1][2].x, h - c[n - 1][2].y);

            for (int i = 0; i < n; i++) {
                switch (tag[i]) {
                case POTRACE_CORNER:
                    fprintf(fp, " L" ACCURACY "," ACCURACY " L" ACCURACY "," ACCURACY,
                        c[i][1].x, h - c[i][1].y, c[i][2].x, h - c[i][2].y);
                    break;
                case POTRACE_CURVETO:
                    fprintf(fp, " C" ACCURACY "," ACCURACY " " ACCURACY "," ACCURACY " " ACCURACY "," ACCURACY,
                        c[i][0].x, h - c[i][0].y, c[i][1].x, h - c[i][1].y, c[i][2].x, h - c[i][2].y);
                    break;
                }
            }
            fprintf(fp, " Z");
            p = p->next;
        }
        fprintf(fp, "\"/>\n");
        fprintf(fp, "</g>\n");
    } else { // EPS
        fprintf(fp, "gsave\n");
        
        while (p != NULL) {
            int n = p->curve.n;
            potrace_dpoint_t (*c)[3] = p->curve.c;
            int *tag = p->curve.tag;

            if (n == 0) {
                p = p->next;
                continue;
            }
            
            fprintf(fp, "%f %f moveto\n", c[n-1][2].x, c[n-1][2].y);
            for (int i=0; i<n; i++) {
                switch (tag[i]) {
                case POTRACE_CORNER:
                    fprintf(fp, "%f %f lineto\n", c[i][1].x, c[i][1].y);
                    fprintf(fp, "%f %f lineto\n", c[i][2].x, c[i][2].y);
                    break;
                case POTRACE_CURVETO:
                    fprintf(fp, "%f %f %f %f %f %f curveto\n",
                            c[i][0].x, c[i][0].y, c[i][1].x, c[i][1].y, c[i][2].x, c[i][2].y);
                    break;
                }
            }
            p = p->next;
        }
        fprintf(fp, "%f %f %f setrgbcolor fill\n", r / 255.0, g / 255.0, b / 255.0);
        fprintf(fp, "grestore\n");
    }
    
    potrace_state_free(st);
    potrace_param_free(param);
    return 0;
}

void color_quant(unsigned char *im, int w, int h, int n_colors, char *name, int flag, int turdsize, double alphamax, double opttolerance)
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

    if (flag&1) stbi_write_jpg("posterized.jpg", w, h, 3, im, 0);
    FILE *fp = fopen(name, "w");
    if (!(flag&32)) fprintf(fp, "%%!PS-Adobe-3.0 EPSF-3.0\n");
    if (!(flag&32)) fprintf(fp, "%%%%BoundingBox: 0 0 %d %d\n", w, h);
    if (flag&32) fprintf(fp, "<svg id=\"illust\" xmlns=\"http://www.w3.org/2000/svg\" width=\"%dpx\" height=\"%dpx\" viewBox=\"0 0 %d %d\">\n", w, h, w, h);
    if (flag&32) fprintf(fp, "<!-- Generator: img2vec by Yuichiro Nakada -->");
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
            }
        }
        if (flag&1) {
            char str[256];
            snprintf(str, sizeof(str), "original_d%02d.png", i);
            stbi_write_png(str, w, h, 3, img, 0);
        }
        if (flag&4) {
            if (got->r==255 && got->g==255 && got->b==255) continue;
        }
        if (flag&2) {
            imgp_dilate(img, w, h, 3, img+w * h *3);
            img2vec(fp, img+w * h *3, w, h, got->r, got->g, got->b, flag, turdsize, alphamax, opttolerance);
        } else {
            img2vec(fp, img, w, h, got->r, got->g, got->b, flag, turdsize, alphamax, opttolerance);
        }
    }
    free(img);
    if (!(flag&32)) fprintf(fp, "%%EOF\n");
    if (flag&32) fprintf(fp, "</svg>\n");
    fclose(fp);

    node_free();
    free(heap.buf);
}

void filter_posterize(unsigned char *img, int width, int height, int levels)
{
    int step = 256 / levels;
    for (int i = 0; i < width * height * 3; i++) {
        unsigned char v = img[i];
        int newv = (v / step) * step + step / 2;
        if (newv > 255) newv = 255;
        img[i] = (unsigned char)newv;
    }
}

double dist(unsigned char *img, int i, int j)
{
    double dr = (double)img[i] - (double)img[j];
    double dg = (double)img[i+1] - (double)img[j+1];
    double db = (double)img[i+2] - (double)img[j+2];
    return dr*dr + dg*dg + db*db;
}

void filter_kmeans(unsigned char *img, int width, int height, int k, int iterations)
{
    int n = width * height;
    unsigned char *centroids = malloc(k * 3);
    int *labels = malloc(n * sizeof(int));

    for (int c = 0; c < k; c++) {
        int idx = (rand() % n) * 3;
        centroids[c*3+0] = img[idx+0];
        centroids[c*3+1] = img[idx+1];
        centroids[c*3+2] = img[idx+2];
    }

    for (int it = 0; it < iterations; it++) {
        for (int p = 0; p < n; p++) {
            double bestd = 1e12;
            int bestc = 0;
            for (int c = 0; c < k; c++) {
                double dr = (double)img[p*3+0] - (double)centroids[c*3+0];
                double dg = (double)img[p*3+1] - (double)centroids[c*3+1];
                double db = (double)img[p*3+2] - (double)centroids[c*3+2];
                double d = dr*dr + dg*dg + db*db;
                if (d < bestd) { bestd = d; bestc = c; }
            }
            labels[p] = bestc;
        }

        long *sumr = calloc(k, sizeof(long));
        long *sumg = calloc(k, sizeof(long));
        long *sumb = calloc(k, sizeof(long));
        int *count = calloc(k, sizeof(int));

        for (int p = 0; p < n; p++) {
            int c = labels[p];
            sumr[c] += img[p*3+0];
            sumg[c] += img[p*3+1];
            sumb[c] += img[p*3+2];
            count[c]++;
        }
        for (int c = 0; c < k; c++) {
            if (count[c] > 0) {
                centroids[c*3+0] = sumr[c] / count[c];
                centroids[c*3+1] = sumg[c] / count[c];
                centroids[c*3+2] = sumb[c] / count[c];
            }
        }
        free(sumr); free(sumg); free(sumb); free(count);
    }

    for (int p = 0; p < n; p++) {
        int c = labels[p];
        img[p*3+0] = centroids[c*3+0];
        img[p*3+1] = centroids[c*3+1];
        img[p*3+2] = centroids[c*3+2];
    }

    free(centroids);
    free(labels);
}

double magic_kernel[4*4] = {
    1/64.0, 3/64.0, 3/64.0, 1/64.0,
    3/64.0, 9/64.0, 9/64.0, 3/64.0,
    3/64.0, 9/64.0, 9/64.0, 3/64.0,
    1/64.0, 3/64.0, 3/64.0, 1/64.0,
};

double gaussian_kernel[3*3] = {
    1.0/16, 2.0/16, 1.0/16,
    2.0/16, 4.0/16, 2.0/16,
    1.0/16, 2.0/16, 1.0/16,
};

double sobel_x_kernel[3*3] = {
    -1.0, 0.0, 1.0,
    -2.0, 0.0, 2.0,
    -1.0, 0.0, 1.0,
};

double sobel_y_kernel[3*3] = {
    -1.0, -2.0, -1.0,
    0.0,  0.0,  0.0,
    1.0,  2.0,  1.0,
};

void usage(FILE* fp, char** argv)
{
    fprintf(fp,
        "Usage: %s [options] file\n\n"
        "Options:\n"
        "-h                 Print this message\n"
        "-o <output name>   Output file name [default: img2vec.eps]\n"
        "-svg               Output file type as SVG [default: eps]\n"
        "-c <num>           Reduce color [default: 32]\n"
        "-b <scale>         Blur image with specified scale\n"
        "-n                 Enable noise removal (Gaussian blur)\n"
        "-e                 Enable edge-preserving blur (blur non-edges)\n"
        "-r <dimension>     Resize image to specified width or height\n"
        "-d                 Enable debug mode\n"
        "-x                 Enable dilation\n"
        "-a                 Enable alpha channel processing\n"
        "-cx <num>          Enable custom bit processing with specified bit value\n"
        "-s <scale>         Apply scaling with specified scale\n"
        "-posterize <num>   Apply posterization with specified levels\n"
        "-kmeans <num>      Apply k-means clustering with specified number of colors\n"
        "-turd <num>        Set turdsize for potrace (removes small paths) [default: 2]\n"
        "-alpha <num>       Set alphamax for potrace (edge smoothness) [default: 1.0]\n"
        "-opttol <num>      Set opttolerance for potrace (curve optimization) [default: 0.2]\n"
        "\n",
        argv[0]);
}

int main(int argc, char* argv[])
{
    char *name = argv[1];
    char *outfile = "img2vec.eps";
    int color = 32;
    int flag = 0;
    float scale = 2;
    int bit = 4;
    int noise_removal = 0;
    int edge_blur = 0;
    float resize = 0;
    int levels = 0;
    int colors = 0;
    int turdsize = 2;
    double alphamax = 1.0;
    double opttolerance = 0.2;

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
            flag |= 1; // debug
        } else if (!strcmp(argv[i], "-x")) {
            flag |= 2; // dilate
        } else if (!strcmp(argv[i], "-a")) {
            flag |= 4; // alpha
        } else if (!strcmp(argv[i], "-b")) {
            flag |= 8; // blur
            scale = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-cx")) {
            flag |= 16;
            bit = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-svg")) {
            flag |= 32;
        } else if (!strcmp(argv[i], "-s")) {
            flag |= 64;
            scale = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-n")) {
            noise_removal = 1;
        } else if (!strcmp(argv[i], "-e")) {
            edge_blur = 1;
        } else if (!strcmp(argv[i], "-r")) {
            resize = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-posterize")) {
            levels = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-kmeans")) {
            colors = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-turd")) {
            turdsize = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-alpha")) {
            alphamax = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-opttol")) {
            opttolerance = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-h")) {
            usage(stderr, argv);
            return 0;
        } else {
            name = argv[i];
        }
    }

    uint8_t *pixels;
    int w, h, bpp;
    pixels = stbi_load(name, &w, &h, &bpp, 3);
    if (!pixels) {
        printf("Error loading image: %s\n", stbi_failure_reason());
        return 1;
    }

    if (resize > 0) {
        int new_w = w * resize;
        int new_h = h * resize;
        printf("resized: %dx%d\n", new_w, new_h);
        uint8_t *resized = malloc(new_w * new_h * 3);
        stbir_resize_uint8_srgb(pixels, w, h, 0, resized, new_w, new_h, 0, 3);
        free(pixels);
        pixels = resized;
        w = new_w;
        h = new_h;
        if (flag&1) stbi_write_jpg("resized.jpg", w, h, 3, pixels, 0);
    }

    if (levels>0) filter_posterize(pixels, w, h, levels);
    if (colors>0) filter_kmeans(pixels, w, h, colors, 10);

    if (flag&8) {
        int sx = w*scale;
        int sy = h*scale;
        uint8_t *posterized = malloc(sx*sy *3 *2);
        stbir_resize_uint8_srgb(pixels, w, h, 0, posterized+sx*sy*3, sx, sy, 0, 3);
        imgp_filter(posterized+sx*sy*3, sx, sy, posterized, magic_kernel, 4, 1, 0);
        stbir_resize_uint8_srgb(posterized, sx, sy, 0, pixels, w, h, 0, 3);
        if (flag&1) stbi_write_jpg("magic.jpg", w, h, 3, pixels, 0);
        free(posterized);
    }

    if (noise_removal) {
        uint8_t *denoised = malloc(w * h * 3);
        imgp_filter(pixels, w, h, denoised, gaussian_kernel, 3, 1, 0);
        memcpy(pixels, denoised, w * h * 3);
        free(denoised);
        if (flag&1) stbi_write_jpg("denoised.jpg", w, h, 3, pixels, 0);
    }

    if (edge_blur) {
        uint8_t *gray = malloc(w * h);
        imgp_gray(pixels, w, h, w, gray, w);
        uint8_t *edge_x = malloc(w * h * 3);
        uint8_t *edge_y = malloc(w * h * 3);
        imgp_filter(pixels, w, h, edge_x, sobel_x_kernel, 3, 1, 0);
        imgp_filter(pixels, w, h, edge_y, sobel_y_kernel, 3, 1, 0);
        uint8_t *edges = malloc(w * h);
        for (int i = 0; i < w * h; i++) {
            int ex = abs(edge_x[i * 3]);
            int ey = abs(edge_y[i * 3]);
            edges[i] = (uint8_t)sqrt(ex * ex + ey * ey);
            if (edges[i] > 255) edges[i] = 255;
        }
        free(edge_x);
        free(edge_y);
        uint8_t *blurred = malloc(w * h * 3);
        imgp_filter(pixels, w, h, blurred, gaussian_kernel, 3, 1, 0);
        for (int i = 0; i < w * h * 3; i++) {
            int idx = i / 3;
            float blend = edges[idx] / 255.0;
            pixels[i] = (uint8_t)(pixels[i] * blend + blurred[i] * (1 - blend));
        }
        free(gray);
        free(edges);
        free(blurred);
        if (flag&1) stbi_write_jpg("edge_blurred.jpg", w, h, 3, pixels, 0);
    }

    if (flag&16) {
        uint8_t *p = pixels;
        for (int n=0; n<w*h*3; n++) {
            *p = ((*p)>>bit)<<bit;
            p++;
        }
    }
    if (flag&64) {
        int sx = w*scale;
        int sy = h*scale;
        uint8_t *posterized = malloc(sx*sy *3);
        if (scale<1) {
            uint8_t *pix = malloc(w*h *3);
            imgp_filter(pixels, w, h, pix, magic_kernel, 4, 1, 0);
            stbir_resize_uint8_srgb(pix, w, h, 0, posterized, sx, sy, 0, 3);
            free(pix);
        } else {
            posterized = malloc(sx*sy *3);
            stbir_resize_uint8_srgb(pixels, w, h, 0, posterized, sx, sy, 0, 3);
        }
        free(pixels);
        pixels = posterized;
        w = sx;
        h = sy;
    }
    color_quant(pixels, w, h, color, outfile, flag, turdsize, alphamax, opttolerance);

    stbi_image_free(pixels);
}

#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
EMSCRIPTEN_KEEPALIVE int process_image(uint8_t *data, int size, int colors, int turdsize, double alphamax, double opttolerance) {
    int w, h, bpp;
    uint8_t *pixels = stbi_load_from_memory(data, size, &w, &h, &bpp, 3);
    if (!pixels) return -1;
    if (w * h > 10000000) {
        stbi_image_free(pixels);
        return -2;
    }
    color_quant(pixels, w, h, colors, "output.svg", 32, turdsize, alphamax, opttolerance);
    stbi_image_free(pixels);
    return 0;
}
#endif
