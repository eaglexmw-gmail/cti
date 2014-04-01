/*
 * A few image types are reference-counted to save memory when used
 * by multiple instances.
 */
#include <stdio.h>
#include <string.h>
#include "Images.h"
#include "Audio.h"
#include "Array.h"
#include "Mem.h"
#include "Cfg.h"
#include "dpf.h"

#ifndef _min
#define _min(a, b)  ((a) < (b) ? (a) : (b))
#endif

#ifndef table_size
#define table_size(x) (sizeof(x)/sizeof(x[0]))
#endif

Gray_buffer *Gray_buffer_new(int width, int height, Image_common *c)
{
  Gray_buffer *gray = Mem_calloc(1, sizeof(Gray_buffer));
  if (c) {
    gray->c = *c;
  }
  gray->width = width;
  gray->height = height;
  gray->data_length = width * height;
  gray->data = Mem_calloc(1, gray->data_length); 	/* Caller must fill in data! */
  return gray;
}


Gray_buffer *PGM_buffer_from(uint8_t *data, int len, Image_common *c)
{
  ArrayU8 *a = ArrayU8_temp_const(data,len); /* FIXME:  Maybe just pass in an ArrayU8? */
  int i = 0, j = 0;
  int x = 0, y = 0, maxval = 0;
  int n;

  ArrayU8 *PGMsignature = ArrayU8_temp_const("P5\n", strlen("P5\n"));
  ArrayU8 *newline = ArrayU8_temp_const("\n", strlen("\n"));

  if (ArrayU8_search(a, 0, PGMsignature) == -1) {
    fprintf(stderr, "PGM signature not found file\n");
    return 0L;
  }

  i = PGMsignature->len;
  while (maxval == 0) {
    j = ArrayU8_search(a, i, newline);
    if (j == -1) {
      fprintf(stderr, "not a PGM file\n");
      return 0L;
    }
    if (!x) {
      n = sscanf((char*)a->data+i, "%d %d", &x, &y);
      if (n == 2) {
	printf("%dx%d\n", x, y);
      }
    }
    else {
      n = sscanf((char*)a->data+i, "%d", &maxval);
      if (n == 1) {
	printf("%d\n", maxval);
      }
    }
    
    if (n == 0 && a->data[i] != '#') {
      fprintf(stderr, "not a PGM file (no comment)\n");
      return 0L;
    }

    i = j+1;
  }

  j += 1;			/* skip newline */

  fprintf(stderr, "PGM: %dx%d %d expected %d found\n", x, y, x*y, len - j);
  if (x*y == len-j) {
    Gray_buffer *gray = Gray_buffer_new(x, y, c);
    memcpy(gray->data, a->data+j, len-j);
    return gray;
  }
  
  return 0L;
}


Gray_buffer *Gray_buffer_from(uint8_t *data, int width, int height, Image_common *c)
{
  Gray_buffer *gray = Gray_buffer_new(width, height, c);
  memcpy(gray->data, data, width*height);
  return gray;
}


void Gray_buffer_discard(Gray_buffer *gray)
{
  Mem_free(gray->data);
  memset(gray, 0, sizeof(*gray));
  Mem_free(gray);
}


Gray32_buffer *Gray32_buffer_new(int width, int height, Image_common *c)
{
  Gray32_buffer *gray32 = Mem_calloc(1, sizeof(Gray32_buffer));
  if (c) {
    gray32->c = *c;
  }
  gray32->width = width;
  gray32->height = height;
  gray32->data_length = width * height;
  gray32->data = Mem_calloc(1, sizeof(*gray32->data) * gray32->data_length); 	/* Caller must fill in data! */
  return gray32;
}

void Gray32_buffer_discard(Gray32_buffer *gray32)
{
  Mem_free(gray32->data);
  memset(gray32, 0, sizeof(*gray32));
  Mem_free(gray32);
}


RGB3_buffer *RGB3_buffer_new(int width, int height, Image_common *c)
{
  RGB3_buffer *rgb = Mem_calloc(1, sizeof(*rgb));
  if (c) {
    rgb->c = *c;
  }
  rgb->width = width;
  rgb->height = height;
  rgb->data_length = width * height * 3;
  rgb->data = Mem_calloc(1, rgb->data_length); 	/* Caller must fill in data! */
  LockedRef_init(&rgb->c.ref);
  return rgb;
}


RGB3_buffer *RGB3_buffer_clone(RGB3_buffer *rgb_src)
{
  RGB3_buffer *rgb = RGB3_buffer_new(rgb_src->width, rgb_src->height, &rgb_src->c);
  memcpy(rgb->data, rgb_src->data, rgb_src->data_length);
  return rgb;  
}


RGB3_buffer *RGB3_buffer_ref(RGB3_buffer *rgb)
{
  int count;
  LockedRef_increment(&rgb->c.ref, &count);
  return rgb;
}


void RGB_buffer_merge_rgba(RGB3_buffer *rgb, uint8_t *rgba, int width, int height, int stride)
{
  int x=0;
  int y;
  int x_in = 0;
  int y_in;
  int offsetx = 0;
  int offsety = 0;
  int copied = 0;

  // printf("stride=%d\n", stride);

  for (y = offsety, y_in = 0; y >= 0 && y < rgb->height && y < (offsety+height) && y_in < height; y++, y_in++) {
    for (x = offsetx, x_in = 0; x >= 0 && x < rgb->width && x < (offsetx+width) && x_in < width; x++, x_in++) {
      uint8_t *prgb = rgb->data + (((y*rgb->width) + x) * 3);
      uint8_t *prgba = rgba + y_in*stride + x_in*4;
      uint8_t r, g, b, a;

      /* Cariro uses native-endian packed 32-bit values */
      b = prgba[0];
      g = prgba[1];
      r = prgba[2];
      a = prgba[3];

      /* RGB3_buffer uses PPM-style interleaved bytes.   Copy data using alpha channel. */
      prgb[0] = (((255 - a) * prgb[0]) + (a * r))/255;
      prgb[1] = (((255 - a) * prgb[1]) + (a * g))/255;
      prgb[2] = (((255 - a) * prgb[2]) + (a * b))/255;

      copied += 1;
    }
  }

  if (0) { 
    FILE *f = fopen("/dev/shm/rgb.ppm", "wb");
    FILE *g = fopen("/dev/shm/a.pgm", "wb");
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    fprintf(g, "P5\n%d %d\n255\n", width, height);
    int y, x;
    uint8_t *p = rgba;
    for (y=0; y < height; y++) {
      for (x=0; x < width; x++) {
	uint8_t data[3];
	data[0] = p[0];
	data[1] = p[1];
	data[2] = p[2];
	if (fwrite(data, 3, 1, f) != 1) { perror("fwrite"); }
	if (fwrite(p+3, 1, 1, g) != 1) { perror("fwrite"); }
	p += 4;
      }
    }
    fclose(g);
    fclose(f);
  }

  dpf("%d pixels copied (%d,%d) to (%d,%d)\n", 
	   copied, 
	   offsetx, offsety,
	   x, y);
}


void RGB3_buffer_discard(RGB3_buffer *rgb)
{
  int count;
  LockedRef_decrement(&rgb->c.ref, &count);
  if (count == 0) {
    Mem_free(rgb->data);
    memset(rgb, 0, sizeof(*rgb));
    Mem_free(rgb);
  }
}


BGR3_buffer *BGR3_buffer_new(int width, int height, Image_common *c)
{
  BGR3_buffer *bgr = Mem_calloc(1, sizeof(BGR3_buffer));
  if (c) {
    bgr->c = *c;
  }
  bgr->width = width;
  bgr->height = height;
  bgr->data_length = width * height * 3;
  bgr->data = Mem_calloc(1, bgr->data_length); 	/* Caller must fill in data! */
  return bgr;
}

void BGR3_buffer_discard(BGR3_buffer *bgr)
{
  Mem_free(bgr->data);
  memset(bgr, 0, sizeof(*bgr));
  Mem_free(bgr);
}


static void swap_0_2(uint8_t *buffer, int length)
{
  int i;
  for (i=0; i < length; i+=3) {
    uint8_t tmp;
    tmp = buffer[i];
    buffer[i] = buffer[i+2];
    buffer[i+2] = tmp;
  }
}


/* Dirty hack... */
void bgr3_to_rgb3(BGR3_buffer **bgr, RGB3_buffer **rgb)
{
  *rgb = (RGB3_buffer*)*bgr;
  *bgr = 0L;
  swap_0_2((*rgb)->data, (*rgb)->data_length);
}

void rgb3_to_bgr3(RGB3_buffer **rgb, BGR3_buffer **bgr)
{
  *bgr = (BGR3_buffer*)*rgb;
  *rgb = 0L;
  swap_0_2((*bgr)->data, (*bgr)->data_length);
}


YUV422P_buffer * YUV422P_buffer_new(int width, int height, Image_common *c)
{
  YUV422P_buffer * yuv422p = Mem_calloc(1, sizeof(*yuv422p));
  if (c) {
    yuv422p->c = *c;
  }
  yuv422p->width = width;
  yuv422p->height = height;
  yuv422p->y_length = width*height;
  yuv422p->cr_width = width/2;
  yuv422p->cr_height = height;
  yuv422p->cr_length = yuv422p->cr_width*yuv422p->cr_height;
  yuv422p->cb_width = width/2;
  yuv422p->cb_height = height;
  yuv422p->cb_length = yuv422p->cb_width*yuv422p->cb_height;
  yuv422p->y = Mem_calloc(1, yuv422p->y_length+32); yuv422p->y[yuv422p->y_length] = 0x55;
  yuv422p->cr = Mem_calloc(1, yuv422p->cr_length+32);  yuv422p->cr[yuv422p->cr_length] = 0x55;
  yuv422p->cb = Mem_calloc(1, yuv422p->cb_length+32);  yuv422p->cb[yuv422p->cb_length] = 0x55;
  LockedRef_init(&yuv422p->c.ref);
  return yuv422p;
}

YUV422P_buffer *YUV422P_buffer_ref(YUV422P_buffer *yuv422p)
{
  int count;
  LockedRef_increment(&yuv422p->c.ref, &count);
  return yuv422p;
}


static struct {
  int len;
  int width;
  int height;
} 
yuv422p_known_sizes[] = {
  { .len = 691200, .width = 720, .height = 480 },
  { .len = 614400, .width = 640, .height = 480 },
  { .len = 518400, .width = 720, .height = 360 },
  { .len = 460800, .width = 640, .height = 360 },
};

YUV422P_buffer *YUV422P_buffer_from(uint8_t *data, int len, Image_common *c)
{
  int i;
  YUV422P_buffer * yuv422p = 0L;
  int width = 0, height;

  for (i=0; i < table_size(yuv422p_known_sizes); i++) {
    if (yuv422p_known_sizes[i].len == len) {
      width = yuv422p_known_sizes[i].width;
      height = yuv422p_known_sizes[i].height;
      printf("%d bytes: %dx%d\n", len, width, height);
      break;
    }
  }

  if (width) {
    yuv422p = YUV422P_buffer_new(width, height, 0L);
    memcpy(yuv422p->y,  data+(width*height*0), width*height);
    memcpy(yuv422p->cr, data+(width*height*2/2), width*height/2);
    memcpy(yuv422p->cb, data+(width*height*3/2), width*height/2);
  }

  return yuv422p;
}


void YUV422P_buffer_discard(YUV422P_buffer *yuv422p)
{
  int count;
  LockedRef_decrement(&yuv422p->c.ref, &count);
  if (count == 0) {
    if (yuv422p->cb[yuv422p->cb_length] != 0x55) { fprintf(stderr, "cb buffer spilled!\n"); }
    Mem_free(yuv422p->cb);
    if (yuv422p->cr[yuv422p->cr_length] != 0x55) { fprintf(stderr, "cr buffer spilled!\n"); }
    Mem_free(yuv422p->cr);
    if (yuv422p->y[yuv422p->y_length] != 0x55) { fprintf(stderr, "y buffer spilled!\n"); }
    Mem_free(yuv422p->y);
    memset(yuv422p, 0, sizeof(*yuv422p));
    Mem_free(yuv422p);
  }
}


static void YUV422P_to_xGx(YUV422P_buffer *yuv422p, RGB3_buffer *rgb, BGR3_buffer *bgr)
{
  // R = Y + 1.402 (Cr-128)
  // G = Y - 0.34414 (Cb-128) - 0.71414 (Cr-128)
  // B = Y + 1.772 (Cb-128)
  
  int x, y;
  uint8_t *prgb = 0L;
  uint8_t *pbgr = 0L;

  if (rgb) {
    prgb = rgb->data;
  }

  if (bgr) {
    pbgr = bgr->data;
  }

  uint8_t *pY = yuv422p->y;
  uint8_t *pCr = yuv422p->cr;
  uint8_t *pCb = yuv422p->cb;

  for (y = 0; y < yuv422p->height; y++) {
    int R, G, B, Y, Cr = 0, Cb = 0;
    for (x = 0; x < yuv422p->width; x+=1) {
      if (x % 2 == 0) {
	pCr++;
	pCb++;
	Cr = *pCr;
	Cb = *pCb;
      }

      Y = *pY;
      R = Y + 1.402 * (Cr - 128);
      G = Y - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128);
      B = Y + 1.772 * (Cb - 128);

      if (R > 255) { R = 255; } else if (R < 0) { R = 0; } 
      if (G > 255) { G = 255; } else if (G < 0) { G = 0; } 
      if (B > 255) { B = 255; } else if (B < 0) { B = 0; } 

      /* FIXME: Should use averaging... */
      if (prgb) {
	prgb[0] = R;
	prgb[1] = G;
	prgb[2] = B;
	prgb += 3;
      }

      if (pbgr) {
	pbgr[0] = B;
	pbgr[1] = G;
	pbgr[2] = R;
	pbgr += 3;
      }
      pY++;
    }
  }
}


RGB3_buffer *YUV422P_to_RGB3(YUV422P_buffer *yuv422p)
{
  RGB3_buffer *rgb = RGB3_buffer_new(yuv422p->width, yuv422p->height, &yuv422p->c);
  YUV422P_to_xGx(yuv422p, rgb, 0L);
  return rgb;
}


BGR3_buffer *YUV422P_to_BGR3(YUV422P_buffer *yuv422p)
{
  BGR3_buffer *bgr = BGR3_buffer_new(yuv422p->width, yuv422p->height, &yuv422p->c);
  YUV422P_to_xGx(yuv422p, 0L, bgr);
  return bgr;
}


YUV422P_buffer *RGB3_toYUV422P(RGB3_buffer *rgb)
{
  //Y = 0.299R + 0.587G + 0.114B
  //U'= (B-Y)*0.565
  //V'= (R-Y)*0.713

  int x, y;
  YUV422P_buffer *yuv422p = YUV422P_buffer_new(rgb->width, rgb->height, &rgb->c);

  uint8_t *p = rgb->data;
  uint8_t *pY = yuv422p->y;
  uint8_t *pCr = yuv422p->cr;
  uint8_t *pCb = yuv422p->cb;

  for (y=0; y < rgb->height; y++) {
    for (x = 0; x < rgb->width; x+=1) {
      uint8_t R = p[0];
      uint8_t G = p[1];
      uint8_t B = p[2];
      uint8_t Y;

      Y = _min(0.299*R + 0.587*G + 0.114*B, 255);
      *pY = Y;

      if (x % 2 == 0) {
	*pCb = _min((B - Y)*0.565 + 128, 255);
	*pCr = _min((R - Y)*0.713 + 128, 255);
	pCr++;
	pCb++;
      }

      p += 3;
      pY++;
    }

  }

  return yuv422p;
}


YUV422P_buffer *BGR3_toYUV422P(BGR3_buffer *bgr)
{
  //Y = 0.299R + 0.587G + 0.114B
  //U'= (B-Y)*0.565
  //V'= (R-Y)*0.713

  int x, y;
  YUV422P_buffer *yuv422p = YUV422P_buffer_new(bgr->width, bgr->height, &bgr->c);

  uint8_t *p = bgr->data;
  uint8_t *pY = yuv422p->y;
  uint8_t *pCr = yuv422p->cr;
  uint8_t *pCb = yuv422p->cb;

  for (y=0; y < bgr->height; y++) {
    for (x = 0; x < bgr->width; x+=1) {
      uint8_t B = p[0];
      uint8_t G = p[1];
      uint8_t R = p[2];
      uint8_t Y;

      Y = _min(0.299*R + 0.587*G + 0.114*B, 255);
      *pY = Y;

      if (x % 2 == 0) {
	*pCb = _min((B - Y)*0.565 + 128, 255);
	*pCr = _min((R - Y)*0.713 + 128, 255);
	pCr++;
	pCb++;
      }

      p += 3;
      pY++;
    }

  }

  return yuv422p;
}


YUV420P_buffer * YUV420P_buffer_new(int width, int height, Image_common *c)
{
  YUV420P_buffer * yuv420p = Mem_calloc(1, sizeof(*yuv420p));
  if (c) {
    yuv420p->c = *c;
  }
  yuv420p->width = width;
  yuv420p->height = height;
  yuv420p->y_length = width*height;
  yuv420p->cr_length = width*height/4;
  yuv420p->cb_length = width*height/4;

  /* One data allocation, component pointers are set within it. */
  yuv420p->data = Mem_calloc(1, yuv420p->y_length+yuv420p->cr_length+yuv420p->cb_length);

  yuv420p->y = yuv420p->data;
  yuv420p->cr = yuv420p->data + yuv420p->y_length;
  yuv420p->cb = yuv420p->data + yuv420p->y_length + yuv420p->cr_length;

  LockedRef_init(&yuv420p->c.ref);  
  return yuv420p;
}


YUV420P_buffer *YUV420P_buffer_ref(YUV420P_buffer *yuv420p)
{
  int count;
  LockedRef_increment(&yuv420p->c.ref, &count);
  return yuv420p;
}


YUV420P_buffer *YUV420P_buffer_from(uint8_t *data, int width, int height, Image_common *c)
{
  YUV420P_buffer * yuv = YUV420P_buffer_new(width, height, c);
  Mem_memcpy(yuv->y, data, yuv->y_length);
  Mem_memcpy(yuv->cb, data + yuv->y_length, yuv->cb_length);
  Mem_memcpy(yuv->cr, data + yuv->y_length + yuv->cb_length, yuv->cr_length);
  return yuv;
}

void YUV420P_buffer_discard(YUV420P_buffer *yuv420p)
{
  int count;
  LockedRef_decrement(&yuv420p->c.ref, &count);
  if (count == 0) {
    Mem_free(yuv420p->data);
    memset(yuv420p, 0, sizeof(*yuv420p));
    Mem_free(yuv420p);
  }
  
}

YUV420P_buffer *YUV422P_to_YUV420P(YUV422P_buffer *yuv422p)
{
  YUV420P_buffer * yuv420p = YUV420P_buffer_new(yuv422p->width, yuv422p->height, &yuv422p->c);
  int x, y;
  int n = 0;

  memcpy(yuv420p->y, yuv422p->y, yuv420p->y_length);

  for (y = 0; y < yuv422p->height; y+=2) {
    for (x = 0; x < yuv422p->width/2; x+=1) {
      yuv420p->cb[n] = (yuv422p->cb[y*yuv422p->width/2 + x] + yuv422p->cb[(y+1)*yuv422p->width/2 + x])/2;
      yuv420p->cr[n] = (yuv422p->cr[y*yuv422p->width/2 + x] + yuv422p->cr[(y+1)*yuv422p->width/2 + x])/2;
      n++;
    }
  }

  // printf("n=%d\n", n);
  return yuv420p;
}


YUV422P_buffer *YUV420P_to_YUV422P(YUV422P_buffer *yuv420p)
{
  YUV422P_buffer * yuv422p = YUV422P_buffer_new(yuv420p->width, yuv420p->height, &yuv420p->c);
  int x, y;
  int n = 0;

  memcpy(yuv422p->y, yuv422p->y, yuv422p->y_length);

  for (y = 0; y < yuv422p->height; y+=2) {
    //for (x = 0; x < yuv422p->width/2; x+=1) {
    //  yuv420p->cb[n] = (yuv422p->cb[y*yuv422p->width/2 + x] + yuv422p->cb[(y+1)*yuv422p->width/2 + x])/2;
    //  yuv420p->cr[n] = (yuv422p->cr[y*yuv422p->width/2 + x] + yuv422p->cr[(y+1)*yuv422p->width/2 + x])/2;
    //  n++;
    //}
  }

  // printf("n=%d\n", n);
  return yuv422p;
}


YUV422P_buffer *YUV422P_copy(YUV422P_buffer *yuv422p, int xoffset, int yoffset, int width, int height)
{
  YUV422P_buffer *newbuf = 0L;
  int x, y;

  if (xoffset+width > yuv422p->width || yoffset+height > yuv422p->height) {
    fprintf(stderr, "copy outside bounds!\n");
    return 0L;
  }

  newbuf = YUV422P_buffer_new(width, height, &yuv422p->c);

  for (y = 0; y < height; y+=1) {
    uint8_t *ysrc = yuv422p->y + ((y + yoffset) * yuv422p->width) + xoffset;
    uint8_t *ydst = newbuf->y + (y*newbuf->width);
    uint8_t *crsrc = yuv422p->cr + ((y + yoffset) * yuv422p->width/2) + (xoffset/2);
    uint8_t *crdst = newbuf->cr + (y*newbuf->width/2);
    uint8_t *cbsrc = yuv422p->cb + ((y + yoffset) * yuv422p->width/2) + (xoffset/2);
    uint8_t *cbdst = newbuf->cb + (y*newbuf->width/2);
    for (x = 0; x < width; x+=1) {
      *ydst++ = *ysrc++;

      if (x % 2 == 0) {
	*crdst++ = *crsrc++;
	*cbdst++ = *cbsrc++;
      }
    }
  }
  
  return newbuf;
}


YUV422P_buffer *YUV422P_clone(YUV422P_buffer *yuv422p)
{
  return YUV422P_copy(yuv422p, 0, 0, yuv422p->width, yuv422p->height);
}


void YUV422P_paste(YUV422P_buffer *dest, YUV422P_buffer *src, int xoffset, int yoffset, int width, int height)
{
  int x, y;

  if (xoffset+width > src->width || yoffset+height > src->height) {
    fprintf(stderr, "copy outside bounds!\n");
    return;
  }

  for (y = 0; y < height; y+=1) {
    uint8_t *ysrc = src->y + ((y + yoffset) * src->width) + xoffset;
    uint8_t *ydst = dest->y + (y*dest->width);
    uint8_t *crsrc = src->cr + ((y + yoffset) * src->width/2) + (xoffset/2);
    uint8_t *crdst = dest->cr + (y*dest->width/2);
    uint8_t *cbsrc = src->cb + ((y + yoffset) * src->width/2) + (xoffset/2);
    uint8_t *cbdst = dest->cb + (y*dest->width/2);
    for (x = 0; x < width; x+=1) {
      *ydst++ = *ysrc++;

      if (x % 2 == 0) {
	*crdst++ = *crsrc++;
	*cbdst++ = *cbsrc++;
      }
    }
  }
}


RGB3_buffer *YUV420P_to_RGB3(YUV420P_buffer *yuv420p)
{
  RGB3_buffer *rgb = RGB3_buffer_new(yuv420p->width, yuv420p->height, &yuv420p->c);

  int x, y;
  int r, g, b;
  int c, d, e;

  int w = yuv420p->width;
  int h = yuv420p->height;
  
  uint8_t *yuvin = yuv420p->data;
  uint8_t *rgbout8 = rgb->data;

  uint8_t *pY;
  uint8_t *pU;
  uint8_t *pV;

  for (y=0; y < h; y+=1) {
    pY = yuvin + (w * y);
    pU = yuvin + (w * h) + (y/2)*(w/2);
    pV = yuvin + (w*h*5/4) + (y/2)*(w/2);

    for (x=0; x < w; x++) {

      c = *pY - 16;
      d = *pU - 128;
      e = *pV - 128;

#define clamp(x) (x < 0 ? 0 : (x > 255 ? 255 : x))

      r = clamp(( 298 * c +   0 * d + 409 * e + 128) >> 8);
      g = clamp(( 298 * c - 100 * d - 208 * e + 128) >> 8);
      b = clamp(( 298 * c + 516 * d +   0 * e + 128) >> 8);

      *rgbout8++ = r;
      *rgbout8++ = g;
      *rgbout8++ = b;

      pY += 1;
      if ((long)pY & 1) {
	pU++;
	pV++;
      }
    }
  }
  
  return rgb;
}


Jpeg_buffer *Jpeg_buffer_new(int size, Image_common *c)
{
  Jpeg_buffer *jpeg = Mem_calloc(1, sizeof(Jpeg_buffer));
  if (c) {
    jpeg->c = *c;
  }
  jpeg->width = -1;
  jpeg->height = -1;
  jpeg->data_length = size;
  jpeg->data = Mem_calloc(1, jpeg->data_length);	/* Caller must fill in data! */
  LockedRef_init(&jpeg->c.ref);
  return jpeg;
}


void Jpeg_buffer_discard(Jpeg_buffer *jpeg)
{
  int count;
  LockedRef_decrement(&jpeg->c.ref, &count);
  if (count == 0) {
    Mem_free(jpeg->data);
    memset(jpeg, 0, sizeof(*jpeg));
    Mem_free(jpeg);
  }
}


Jpeg_buffer *Jpeg_buffer_ref(Jpeg_buffer *jpeg)
{
  int count;
  LockedRef_increment(&jpeg->c.ref, &count);
  return jpeg;
}



O511_buffer *O511_buffer_new(int width, int height, Image_common *c)
{
  O511_buffer *o511 = Mem_calloc(1, sizeof(O511_buffer));
  if (c) {
    o511->c = *c;
  }
  o511->width = width;
  o511->height = height;
  o511->data_length = (width*height*3)/2;
  o511->data = Mem_calloc(1, o511->data_length);

  return o511;
}

O511_buffer *O511_buffer_from(uint8_t *data, int data_length, int width, int height, Image_common *c)
{
  O511_buffer *o511 = Mem_calloc(1, sizeof(O511_buffer));
  if (c) {
    o511->c = *c;
  }
  o511->width = width;
  o511->height = height;
  o511->data_length = data_length;
  o511->data = Mem_calloc(1, o511->data_length);
  memcpy(o511->data, data, data_length);
  o511->encoded_length = o511->data_length; /* same as data_length */

  return o511;
}

void O511_buffer_discard(O511_buffer *o511)
{
  Mem_free(o511->data);
  memset(o511, 0, sizeof(*o511));
  Mem_free(o511);
}


H264_buffer *H264_buffer_from(uint8_t *data, int data_length, int width, int height, Image_common *c)
{
  H264_buffer *h264 = Mem_calloc(1, sizeof(H264_buffer));
  if (c) {
    h264->c = *c;
  }
  h264->width = width;
  h264->height = height;
  h264->data_length = data_length;
  h264->data = Mem_calloc(1, h264->data_length);
  memcpy(h264->data, data, data_length);
  h264->encoded_length = h264->data_length; /* same as data_length */

  return h264;
}


void H264_buffer_discard(H264_buffer *h264)
{
  Mem_free(h264->data);
  memset(h264, 0, sizeof(*h264));
  Mem_free(h264);
}



ImageType Image_guess_type(uint8_t *data, int len)
{
  ImageType t = IMAGE_TYPE_UNKNOWN;
  if (len > 4) {
    if (data[0] == 'P' && data[1] == '5' && data[2] == '\n') {
      t = IMAGE_TYPE_PGM;      
    }
    else if (data[0] == 'P' && data[1] == '6' && data[2] == '\n') {
      t = IMAGE_TYPE_PPM;
    }
    else if (data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff && data[3] == 0xe0) {
      t = IMAGE_TYPE_JPEG;
    }
    else if (data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff && data[3] == 0xe1) {
      t = IMAGE_TYPE_JPEG;
    }
    else if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01) {
      t = IMAGE_TYPE_H264;
    }
    else if (data[0] == 0xff && data[1] == 0xf1 && data[2] == 0x5c && data[3] == 0x40) {
      /* Confirmed format at http://www.p23.nl/projects/aac-header/ */
      t = AUDIO_TYPE_AAC;
    }
  }

  return t;
}
