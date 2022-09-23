#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include <stb_image_write.h>
#include <stb_image.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define ABS(x) ((x) < 0 ? -(x) : (x))

#define CAY_PIXEL_SIZE 32
#define CAY_DIVIDE 4

typedef struct color_t color_t;
typedef struct image_t image_t;

struct color_t {
  int h, s, l;
};

struct image_t {
  int width, height;
  color_t *data;
  
  color_t color;
};

color_t uint_to_color(uint32_t value) {
  float red = ((value >> 0) & 0xFF) / 255.0f;
  float green = ((value >> 8) & 0xFF) / 255.0f;
  float blue = ((value >> 16) & 0xFF) / 255.0f;
  
  float max = red;
  if (green > max) max = green;
  if (blue > max) max = blue;
  
  float min = red;
  if (green < min) min = green;
  if (blue < min) min = blue;
  
  float h, s, l;
  l = (max + min) * 0.5f;
  
  if (max == min) {
    return (color_t){0, 0, l * 255.0f};
  }
  
  s = (max - min) / (1.0f - ABS(2.0f * l - 1.0f));
  
  if (max == red) {
    h = 0 + ((green - blue) * 60.0f) / (max - min);
    if (h < 0) h += 360.0f;
  } else if (max == green) {
    h = 120.0f + ((blue - red) * 60.0f) / (max - min);
  } else {
    h = 240.0f + ((red - green) * 60.0f) / (max - min);
    if (h >= 360.0f) h -= 360.0f;
  }
  
  return (color_t){h, s * 255.0f, l * 255.0f};
}

uint32_t rgb_to_uint(int red, int green, int blue) {
  if (red < 0) red = 0;
  if (green < 0) green = 0;
  if (blue < 0) blue = 0;
  
  if (red > 255) red = 255;
  if (green > 255) green = 255;
  if (blue > 255) blue = 255;
  
  return (uint32_t)(red) + (uint32_t)(green << 8) + (uint32_t)(blue << 16) + (uint32_t)(0xFF << 24);
}

uint32_t color_to_uint(color_t color) {
  float chroma = (1.0f - fabs(2.0f * (color.l / 255.0f) - 1.0f)) * (color.s / 255.0f);
  float hue = color.h / 60.0f;
  
  float value = chroma * (1.0f - fabs(fmod(hue, 2.0f) - 1.0f));
  
  float red = 0.0f;
  float green = 0.0f;
  float blue = 0.0f;
  
  if (hue >= 0.0f && hue < 1.0f) {
    red = chroma;
    green = value;
  } else if (hue >= 1.0f && hue < 2.0f) {
    green = chroma;
    red = value;
  } else if (hue >= 2.0f && hue < 3.0f) {
    green = chroma;
    blue = value;
  } else if (hue >= 3.0f && hue < 4.0f) {
    blue = chroma;
    green = value;
  } else if (hue >= 4.0f && hue < 5.0f) {
    blue = chroma;
    red = value;
  } else if (hue >= 5.0f && hue < 6.0f) {
    red = chroma;
    blue = value;
  }
  
  float light = (color.l / 255.0f) - chroma * 0.5f;
  return rgb_to_uint((red + light) * 255.0f, (green + light) * 255.0f, (blue + light) * 255.0f);
}

float color_dist(color_t a, color_t b) {
  return sqrtf((a.h - b.h) * (a.h - b.h) + 1.5f * (a.s - b.s) * (a.s - b.s) + (a.l - b.l) * (a.l - b.l));
}

image_t image_load(const char *path, int size) {
  int width, height, bpp;
  
  uint32_t *raw_data = stbi_load(path, &width, &height, &bpp, 4);
  if (!raw_data) exit(1);
  
  int new_width = size ? size : (width / CAY_DIVIDE);
  int new_height = size ? size : (height / CAY_DIVIDE);
  
  color_t *data = malloc(new_width * new_height * sizeof(color_t));
  color_t color = (color_t){0, 0, 0};
  
  for (int y = 0; y < new_height; y++) {
    for (int x = 0; x < new_width; x++) {
      int rel_x = (x * width) / new_width;
      int rel_y = (y * height) / new_height;
      
      data[x + y * new_width] = uint_to_color(raw_data[rel_x + rel_y * width]);
      
      color.h += data[x + y * new_width].h;
      color.s += data[x + y * new_width].s;
      color.l += data[x + y * new_width].l;
    }
  }
  
  color.h /= new_width * new_height;
  color.s /= new_width * new_height;
  color.l /= new_width * new_height;
  
  stbi_image_free(raw_data);
  return (image_t){new_width, new_height, data, color};
}

void image_save(const char *path, image_t image) {
  uint32_t *raw_data = malloc(image.width * image.height * sizeof(uint32_t));
  
  for (int y = 0; y < image.height; y++) {
    for (int x = 0; x < image.width; x++) {
      raw_data[x + y * image.width] = color_to_uint(image.data[x + y * image.width]);
    }
  }
  
  stbi_write_png(path, image.width, image.height, 4, raw_data, image.width * sizeof(uint32_t));
  free(raw_data);
}

image_t image_empty(int width, int height) {
  return (image_t){width, height, malloc(width * height * sizeof(color_t)), (color_t){0, 0, 0}};
}

void image_copy(image_t dest, image_t src, int x, int y, color_t blend, float factor) {
  for (int rel_y = 0; rel_y < src.height; rel_y++) {
    for (int rel_x = 0; rel_x < src.width; rel_x++) {
      color_t new_color = src.data[rel_x + rel_y * src.width];
      
      new_color.h = (new_color.h * (1.0f - factor)) + (blend.h * factor);
      new_color.s = (new_color.s * (1.0f - factor)) + (blend.s * factor);
      new_color.l = (new_color.l * (1.0f - factor)) + (blend.l * factor);
      
      dest.data[(x + rel_x) + (y + rel_y) * dest.width] = new_color;
    }
  }
}

image_t *images = NULL;
int count = 0;

int main(int argc, const char **argv) {
  if (argc < 2) return 1;
  
  count = argc - 1;
  images = malloc(count * sizeof(image_t));
  
  for (int i = 1; i < argc; i++) {
    printf("loading image %d/%d\n", i, count);
    images[i - 1] = image_load(argv[i], i > 1 ? CAY_PIXEL_SIZE : 0);
  }
  
  image_t output = image_empty(images[0].width * CAY_PIXEL_SIZE, images[0].height * CAY_PIXEL_SIZE);
  
  for (int y = 0; y < images[0].height; y++) {
    for (int x = 0; x < images[0].width; x++) {
      int best_index = 0;
      float best_dist = 1048576.0f;
      
      printf("plotting %d/%d\n", x + y * images[0].width + 1, images[0].width * images[0].height);
      
      for (int i = 1; i < count; i++) {
        float dist = color_dist(images[i].color, images[0].data[x + y * images[0].width]);
        dist += 32.0f * (float)(fmod(rand(), 65537.0f) / 65537.0f);
        
        if (dist < best_dist) {
          best_index = i;
          best_dist = dist;
        }
      }
      
      image_copy(output, images[best_index], x * CAY_PIXEL_SIZE, y * CAY_PIXEL_SIZE, images[0].data[x + y * images[0].width], 0.5f);
    }
  }
  
  printf("saving image\n");
  
  image_save("output.png", output);
  return 0;
}
