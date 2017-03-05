/*
O(n * r^2)
Reference:
  [http://blog.ivank.net/fastest-gaussian-blur.html]
  [http://elynxsdk.free.fr/ext-docs/Blur/Fast_box_blur.pdf]
  [http://www.peterkovesi.com/matlabfns/]

RGB ONLY
The reason for using subscript code like this:

  rgb[0] += ( r + 1 ) * firstv[0];
  rgb[1] += ( r + 1 ) * firstv[1];
  rgb[2] += ( r + 1 ) * firstv[2];

instead of for-loop is for-loop requires stack exchange which
incrases about 100ms cpu time on my mac.

This hard code subscript is also why it handles RGB only, though it
won't modify alpha information.
*/

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>

#define max(x, y) ((x) > (y) ? (x) : (y))
#define min(x, y) ((x) > (y) ? (y) : (x))

void
boxblur_rgb(unsigned char *data, int width, int height, int sigma, int channel);

void
blurbox_sizes(int *sizes, float sigma, int n);

void
boxblur(float *scl, float *tcl, int w, int h, int r, channel);

int
main(int argc, char const *argv[]) {

  if ( argc != 3 ) {
    printf("usage:[img filename sigma]\n");
    exit(1);
  }

  int width, height, channel;
  unsigned char* image;
  image = stbi_load(argv[1], &width, &height, &channel, STBI_rgb);

  if (image == NULL) {
    printf("cant load image\n");
    exit(1);
  }

  float sigma;
  sscanf(argv[2], "%f", &sigma);

  boxblur_rgb(image, width, height, sigma, channel);

  stbi_write_png("test.png", width, height, channel, image, channel * width);
  stbi_image_free(image);

  return 0;
}

void
blurbox_sizes(int *sizes, float sigma, int n) {
  float wideal = sqrt((12 * sigma * sigma / n) + 1);
  int wl = floor(wideal);
  if (wl % 2 == 0)
    wl--;
  int wu = wl + 2;

  double mideal = (12 * sigma * sigma - n * wl * wl - 4 * n * wl - 3 * n) / (-4 * wl - 4);
  int m = round(mideal);

  for (size_t i = 0; i < n; i++)
    sizes[i] = i < m ? wl : wu;
}

void
boxblur_rgb(unsigned char *data, int width, int height, int sigma, channel) {

  int imgsize = width * height * channel;

  int sizes[3] = {0};
  blurbox_sizes(sizes, sigma, 3);

  float *scl = (float *)malloc(imgsize * sizeof(float));
  float *tcl = (float *)malloc(imgsize * sizeof(float));

  if ( tcl == NULL || scl == NULL ) {
    perror("cannot malloc");
    if ( tcl != NULL )
      free(tcl);
    if ( scl != NULL )
      free(scl);
    return;
  }

  for (size_t i = 0; i < imgsize; i++)
     scl[i] = (float)data[i];

  struct rusage begin, end;

  getrusage(RUSAGE_SELF, &begin);

  boxblur(scl, tcl, width, height, (sizes[0] - 1) / 2, channel);
  boxblur(tcl, scl, width, height, (sizes[1] - 1) / 2, channel);
  boxblur(scl, tcl, width, height, (sizes[2] - 1) / 2, channel);

  getrusage(RUSAGE_SELF, &end);

  float ut = (end.ru_utime.tv_sec + end.ru_utime.tv_usec / 1000000.0f) -
             (begin.ru_utime.tv_sec + begin.ru_utime.tv_usec / 1000000.0f);

  float st = (end.ru_stime.tv_sec + end.ru_stime.tv_usec / 1000000.0f) -
             (begin.ru_stime.tv_sec + begin.ru_stime.tv_usec / 1000000.0f);

  printf("users time: %10.2f\nsystem time: %10.2f\n", ut, st);

  for (size_t i = 0; i < imgsize; i++)
     data[i] = (unsigned char)min(255.0f, max(0, round(tcl[i])));

  free(scl);
  free(tcl);
}

void
boxblur(float *scl, float *tcl, int w, int h, int r, int channel) {

  for (size_t i = 0; i < w * h * channel; i++)
    tcl[i] = scl[i];

  float weight = (r+r+1);

  /* horizontal blur */
  ssize_t width = w * channel;

  for (ssize_t ih = 0; ih < h; ih++) {
    ssize_t crowindex = ih * width;                          /* index of header of current row */
    for (ssize_t iw = 0; iw < w; iw++) {
      float rgb[3] = {0};
      float *cpixel = scl + crowindex + iw * channel;      /* current pixel */

      for (ssize_t ix = iw - r; ix < iw + r + 1; ++ix) {
          ssize_t x = min(max(ix, 0), w - 1) * channel;
          rgb[0] += tcl[crowindex + x];
          rgb[1] += tcl[crowindex + x + 1];
          rgb[2] += tcl[crowindex + x + 2];
      }

      cpixel[0] = rgb[0] / weight;
      cpixel[1] = rgb[1] / weight;
      cpixel[2] = rgb[2] / weight;
    }
  }

  /* total blur */
  for (ssize_t iw = 0; iw < w; iw++) {
    ssize_t ccolindex = iw * channel;                           /* row offet of current cloumn */
    for (ssize_t ih = 0; ih < h; ih++) {
      float rgb[3] = {0};
      float *cpixel = tcl + ccolindex + ih * width;

      for (ssize_t iy = ih - r; iy < ih + r + 1; ++iy) {
          int y = min(max(iy, 0), h - 1) * width;

          rgb[0] += scl[y + ccolindex];
          rgb[1] += scl[y + ccolindex + 1];
          rgb[2] += scl[y + ccolindex + 2];
      }
      cpixel[0] = rgb[0] / weight;
      cpixel[1] = rgb[1] / weight;
      cpixel[2] = rgb[2] / weight;
    }
   }

}
