/*
O(n)
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
boxblur_ht(float *scl, float *tcl, int w, int h, int r, int channel);

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
boxblur_rgb(unsigned char *data, int width, int height, int sigma, int channel) {

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

  boxblur_ht(scl, tcl, width, height, (sizes[0] - 1) / 2, channel);
  boxblur_ht(tcl, scl, width, height, (sizes[1] - 1) / 2, channel);
  boxblur_ht(scl, tcl, width, height, (sizes[2] - 1) / 2, channel);

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
boxblur_ht(float *scl, float *tcl, int w, int h, int r, int channel) {

  for (size_t i = 0; i < w * h * channel; i++)
    tcl[i] = scl[i];

  float ave = 1.0f / ( r + r + 1.0f );

  /* horizontal blur [tcl -> scl] */
  ssize_t width = w * channel;

  for (ssize_t ih = 0; ih < h; ih++) {
    float rgb[3] = {0};

    int   coffset       = ih * width;                            /* offset of the first value of a row */
    float *currentpixel = scl + coffset;                         /* pointer to current pixel */
    float *const firstv = tcl + coffset;                         /* pointer to the first value */
    float *const lastv  = firstv + (w - 1) * channel;            /* pointer to the last value */

    float *addp = firstv + r * channel;                         /* pointer to value required by addition */
    float *subp = firstv;                                       /* pointer to value required by subtraction */

    /* --- begin --- calculate header value of row */
    rgb[0] += ( r + 1 ) * firstv[0];
    rgb[1] += ( r + 1 ) * firstv[1];
    rgb[2] += ( r + 1 ) * firstv[2];

    for (size_t i = 0; i < r; i++) {
      rgb[0] += (firstv + i * channel)[0];
      rgb[1] += (firstv + i * channel)[1];
      rgb[2] += (firstv + i * channel)[2];
    }
    /* ---- end ---- calculate header value of row */

    /* calculate values within first "r" range */
    for (size_t i = 0; i <= r; i++) {

      rgb[0] += *(addp++) - firstv[0];
      rgb[1] += *(addp++) - firstv[1];
      rgb[2] += *(addp++) - firstv[2];

      *(currentpixel++) = rgb[0] * ave;
      *(currentpixel++) = rgb[1] * ave;
      *(currentpixel++) = rgb[2] * ave;
    }

    /* calculate values between firts and last "r" range */
    for (size_t i = r+1; i < w - r; i++) {

      rgb[0] += *(addp++) - *(subp++);
      rgb[1] += *(addp++) - *(subp++);
      rgb[2] += *(addp++) - *(subp++);

      *(currentpixel++) = rgb[0] * ave;
      *(currentpixel++) = rgb[1] * ave;
      *(currentpixel++) = rgb[2] * ave;
    }

    /* calculate values of last "r" range */
    for (size_t i = 0; i < r; i++) {

      rgb[0] += lastv[0] - *(subp++);
      rgb[1] += lastv[1] - *(subp++);
      rgb[2] += lastv[2] - *(subp++);

      *(currentpixel++) = rgb[0] * ave;
      *(currentpixel++) = rgb[1] * ave;
      *(currentpixel++) = rgb[2] * ave;
    }
  }

  /* total blur [scl -> tcl] */
  for (ssize_t iw = 0; iw < w; iw++) {
    float rgb[3] = {0};

    int   coffset       = width;                             /* offset of a column */
    float *const firstv = scl + iw * channel;                /* pointer to the first value (of current column)*/
    float *const lastv  = firstv + (h - 1) * width;          /* pointer to the last value (of current column) */
    float *currentpixel = tcl + iw * channel;                /* pointer to current pixel */

    float *addp = firstv + r * coffset;                      /* pointer to value required by addition */
    float *subp = firstv;                                    /* pointer to value required by subtraction */

    /* --- begin --- calculate header value of column */
    rgb[0] += ( r + 1 ) * firstv[0];
    rgb[1] += ( r + 1 ) * firstv[1];
    rgb[2] += ( r + 1 ) * firstv[2];

    for (size_t i = 0; i < r; i++) {
      float *pixel = (firstv + i * coffset);
      rgb[0] += pixel[0];
      rgb[1] += pixel[1];
      rgb[2] += pixel[2];
    }
    /* ---- end ---- calculate header value of column */

    /* calculate values within first "r" range */
    for (size_t i = 0; i <= r; i++) {

      rgb[0] += addp[0] - firstv[0];
      rgb[1] += addp[1] - firstv[1];
      rgb[2] += addp[2] - firstv[2];

      currentpixel[0] = rgb[0] * ave;
      currentpixel[1] = rgb[1] * ave;
      currentpixel[2] = rgb[2] * ave;

      addp += coffset;
      currentpixel += coffset;
    }

    /* calculate values between firts and last "r" range */
    for (size_t i = r+1; i < h - r; i++) {

      rgb[0] += addp[0] - subp[0];
      rgb[1] += addp[1] - subp[1];
      rgb[2] += addp[2] - subp[2];

      currentpixel[0] = rgb[0] * ave;
      currentpixel[1] = rgb[1] * ave;
      currentpixel[2] = rgb[2] * ave;

      addp += coffset;
      subp += coffset;
      currentpixel += coffset;
    }

    /* calculate values of last "r" range */
    for (size_t i = 0; i < r; i++) {

      rgb[0] += lastv[0] - subp[0];
      rgb[1] += lastv[1] - subp[1];
      rgb[2] += lastv[2] - subp[2];

      currentpixel[0] = rgb[0] * ave;
      currentpixel[1] = rgb[1] * ave;
      currentpixel[2] = rgb[2] * ave;

      subp += coffset;
      currentpixel += coffset;
    }
  }

}
