#ifndef _IMAGES_H_
#define _IMAGES_H_

#include <stdint.h>
#include <sys/time.h>
#include "Mem.h"
#include "locks.h"

/* http://en.wikipedia.org/wiki/Layers_%28digital_image_editing%29 */
typedef struct {
  /* I might not use this, but instead use specific buffer types.  That will avoid
     lots of if/else switching.  C++ might have helped there, I admit, but I'm doing
     this project in C. */
} Layer;


/* Enumerate image types, CTI internal use only. */
typedef enum {
  IMAGE_TYPE_UNKNOWN,
  IMAGE_TYPE_JPEG,
  IMAGE_TYPE_PGM,
  IMAGE_TYPE_PPM,
  IMAGE_TYPE_YUV422P,		/* raw dumps from V4L2Capture module */
  IMAGE_TYPE_O511,
  IMAGE_TYPE_H264,		/* One unit of compressor output */
  AUDIO_TYPE_AAC,
} ImageType;

extern ImageType Image_guess_type(uint8_t * data, int len);

enum { 
  IMAGE_INTERLACE_NONE,
  IMAGE_INTERLACE_TOP_FIRST,
  IMAGE_INTERLACE_BOTTOM_FIRST,
  IMAGE_FIELDSPLIT_TOP_FIRST,
  IMAGE_FIELDSPLIT_BOTTOM_FIRST,
};

/* Common metadata fields. */
typedef struct {
  struct timeval tv;
  float nominal_period;
  int interlace_mode;
  int eof;			/* EOF marker. */
  LockedRef ref;
} Image_common;


/* Gray buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;
  Image_common c;
} Gray_buffer;


/* Gray 32-bit buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint32_t *data;
  int data_length;
  Image_common c;
} Gray32_buffer;


/* RGB3 buffer. */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;
  Image_common c;
} RGB3_buffer;


/* BGR3 buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;
  Image_common c;
} BGR3_buffer;


/* YUV422P buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *y;
  int y_length;

  uint8_t *cb;
  int cb_width;
  int cb_height;
  int cb_length;

  uint8_t *cr;
  int cr_width;
  int cr_height;
  int cr_length;

  Image_common c;
} YUV422P_buffer;


/* 420P buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  /* "data" contains all components; y,cb,cr point into this */
  uint8_t *data;		
  uint8_t *y;
  int y_length;
  uint8_t *cb;
  int cb_length;
  uint8_t *cr;
  int cr_length;

  Image_common c;
} YUV420P_buffer;


/* Jpeg buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;		/* Provisional. */
  int encoded_length;		/* Actual encoded jpeg size. */

  Image_common c;
} Jpeg_buffer;


/* O511 buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;		/* Provisional. */
  int encoded_length;		/* Actual encoded size. */

  Image_common c;
} O511_buffer;


/* H264 buffer */
typedef struct {
  MemObject mo;
  int width;
  int height;
  uint8_t *data;
  int data_length;		/* Provisional. */
  int encoded_length;		/* Actual encoded size. */

  Image_common c;
} H264_buffer;


extern Gray_buffer *Gray_buffer_new(int width, int height, Image_common *c);
extern Gray_buffer *PGM_buffer_from(uint8_t *data, int len, Image_common *c);
extern Gray_buffer *Gray_buffer_from(uint8_t *data, int width, int height, Image_common *c);
extern void Gray_buffer_discard(Gray_buffer *gray);

extern Gray32_buffer *Gray32_buffer_new(int width, int height, Image_common *c);
extern void Gray32_buffer_discard(Gray32_buffer *gray);
extern RGB3_buffer *PPM_buffer_from(uint8_t *data, int len, Image_common *c);
extern RGB3_buffer *RGB3_buffer_new(int width, int height, Image_common *c);
extern RGB3_buffer *RGB3_buffer_clone(RGB3_buffer *rgb);
extern RGB3_buffer *RGB3_buffer_ref(RGB3_buffer *rgb);
extern void RGB3_buffer_discard(RGB3_buffer *rgb);
extern void RGB_buffer_merge_rgba(RGB3_buffer *rgb, uint8_t *rgba, int width, int height, int stride);

extern BGR3_buffer *BGR3_buffer_new(int width, int height, Image_common *c);
extern void BGR3_buffer_discard(BGR3_buffer *rgb);

/* In-place conversion. */
void bgr3_to_rgb3(BGR3_buffer **bgr, RGB3_buffer **rgb);
void rgb3_to_bgr3(RGB3_buffer **rgb, BGR3_buffer **bgr);


extern YUV422P_buffer *YUV422P_buffer_new(int width, int height, Image_common *c);
extern YUV422P_buffer *YUV422P_buffer_from(uint8_t *data, int len, Image_common *c);
extern void YUV422P_buffer_discard(YUV422P_buffer *y422p);
extern YUV422P_buffer *YUV422P_copy(YUV422P_buffer *y422p, int x, int y, int width, int height);
extern YUV422P_buffer *YUV422P_clone(YUV422P_buffer *y422p);
extern void YUV422P_paste(YUV422P_buffer *dest, YUV422P_buffer *src, int x, int y, int width, int height);
extern RGB3_buffer *YUV422P_to_RGB3(YUV422P_buffer *y422p);
extern BGR3_buffer *YUV422P_to_BGR3(YUV422P_buffer *y422p);

extern YUV420P_buffer *YUV420P_buffer_new(int width, int height, Image_common *c);
extern YUV420P_buffer *YUV420P_buffer_from(uint8_t *data, int width, int height, Image_common *c);
extern void YUV420P_buffer_discard(YUV420P_buffer *y420p);
extern RGB3_buffer *YUV420P_to_RGB3(YUV420P_buffer *y420p);

extern YUV420P_buffer *YUV422P_to_YUV420P(YUV422P_buffer *y422p);

extern YUV422P_buffer *RGB3_toYUV422P(RGB3_buffer *rgb);
extern YUV422P_buffer *BGR3_toYUV422P(BGR3_buffer *bgr);

extern Jpeg_buffer *Jpeg_buffer_new(int size, Image_common *c);
extern Jpeg_buffer *Jpeg_buffer_from(uint8_t *data, int data_length, Image_common *c); /* DJpeg.c */
extern void Jpeg_buffer_discard(Jpeg_buffer *jpeg);
extern Jpeg_buffer *Jpeg_buffer_ref(Jpeg_buffer *jpeg);

extern O511_buffer *O511_buffer_new(int width, int height, Image_common *c);
extern O511_buffer *O511_buffer_from(uint8_t *data, int data_length, int width, int height, Image_common *c);
extern void O511_buffer_discard(O511_buffer *o511);

extern H264_buffer *H264_buffer_from(uint8_t *data, int data_length, int width, int height, Image_common *c);
extern void H264_buffer_discard(H264_buffer *h264);

#endif
