/*
O(n * r^2)
Super Slow.
*/

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define max(x, y) (x > y ? x : y)
#define min(x, y) (x > y ? y : x)

void
gaussblur_rgb(unsigned char *data, double *kernel, int width, int height, int radius);

int
EDGE(int i, int r, int len);

void
generateKernel(double *kernel, int radius);

int
main(int argc, char const *argv[]) {
  if ( argc != 3 ) {
    printf("usage:[img file radius]\n");
    exit(1);
  }

  int width, height, channel;
  unsigned char* image;
  image = stbi_load(argv[1], &width, &height, &channel, STBI_rgb);

  if (image == NULL) {
    printf("cant load image\n");
    exit(1);
  }

  int radius;
  sscanf(argv[2], "%d", &radius);
  radius = max(0, radius);
  double kernel[radius * 2 + 1];

  generateKernel(kernel, radius);

  gaussblur_rgb(image, kernel, width, height, radius);

  stbi_write_png("test.png", width, height, STBI_rgb, image, STBI_rgb * width);
  stbi_image_free(image);

  return 0;
}

void
gaussblur_rgb(unsigned char *data, double *kernel, int width, int height, int radius) {

  const int rgb = 3;

  double *bitbuf = (double *)malloc(width * height * rgb * sizeof(double));
  if (bitbuf == NULL) {
    fprintf(stderr, "no space for malloc()");
    return;
  }

  bzero(bitbuf, sizeof(bitbuf));

  int width_stride = width * rgb;

  // horizontal blur
  for (size_t i = 0; i < height; i++) {
    unsigned char *pixel_row = data + (i * width_stride);
    for (size_t j = 0; j < width; j++) {
      size_t currentindex = (i * width_stride + j * rgb);
      double *pixel_buf = bitbuf + currentindex;

      for (ssize_t r = -radius, n = 0; r < radius + 1; r++, n++) {
        int row_offset = EDGE(r, j, width) * rgb;
        pixel_buf[0] += (pixel_row + row_offset)[0] * kernel[n];
        pixel_buf[1] += (pixel_row + row_offset)[1] * kernel[n];
        pixel_buf[2] += (pixel_row + row_offset)[2] * kernel[n];
     }

    }
  }

  // total blur(vertical)
  for (size_t i = 0; i < height; i++) {
    for (size_t j = 0; j < width; j++) {
      size_t currentindex  = (i * width_stride + j * rgb);
      double *pixel_colum  = bitbuf + (j * rgb);
      double *pixel_buf    = bitbuf + currentindex;
      unsigned char *pixel = data + currentindex;

      double red = 0, g = 0, b = 0;
      for (ssize_t r = -radius, n = 0; r < radius + 1; r++, n++) {
        int colum_offset = EDGE(r, i, height) * width_stride;
        red += (pixel_colum + colum_offset)[0] * kernel[n];
        g   += (pixel_colum + colum_offset)[1] * kernel[n];
        b   += (pixel_colum + colum_offset)[2] * kernel[n];
     }

     pixel[0] = (unsigned char)min(255.0f, max(0.0f, round(red)));
     pixel[1] = (unsigned char)min(255.0f, max(0.0f, round(g)));
     pixel[2] = (unsigned char)min(255.0f, max(0.0f, round(b)));
    }
  }

  free(bitbuf);

}

int
EDGE(int i, int r, int len) {
  int result = i + r;
  if ( result < 0 )
    return 0;
  if ( result >= len )
    return len - 1;
  return result;
}

void
generateKernel(double *kernel, int radius) {
  if ( radius == 0 ) {
    *kernel = 1;
    return;
  }
  double sigma = radius / 3.0f;
  double sigmap = sqrt((double)2 * M_PI) * sigma;
  double sigma2 = sigma * sigma * 2.0f;

  for (int i = -radius, n = 0; i < radius + 1; ++i, ++n ) {
    double i2 = i * i;
    kernel[n] = exp(-i2 / sigma2) / sigmap;
  }
}
