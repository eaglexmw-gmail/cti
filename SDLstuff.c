/*
 * SDL video output, keyboard input.
 */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */
#include <unistd.h>
#include <math.h>               /* fabs */
#include <SDL.h>
#include <gl.h>
// #include <glu.h>
#include <time.h>               /* clock_gettime */

#include "CTI.h"
#include "Images.h"
#include "Cfg.h"
#include "Keycodes.h"
#include "Pointer.h"
#include "VSmoother.h"
#include "Global.h"


static int SDLtoCTI_Keymap[SDLK_LAST] = {
  [SDLK_UP] = CTI__KEY_UP,
  [SDLK_DOWN] = CTI__KEY_DOWN,
  [SDLK_LEFT] = CTI__KEY_LEFT,
  [SDLK_RIGHT] = CTI__KEY_RIGHT,
  [SDLK_0] = CTI__KEY_0,
  [SDLK_1] = CTI__KEY_1,
  [SDLK_2] = CTI__KEY_2,
  [SDLK_3] = CTI__KEY_3,
  [SDLK_4] = CTI__KEY_4,
  [SDLK_5] = CTI__KEY_5,
  [SDLK_6] = CTI__KEY_6,
  [SDLK_7] = CTI__KEY_7,
  [SDLK_8] = CTI__KEY_8,
  [SDLK_9] = CTI__KEY_9,
  [SDLK_a] = CTI__KEY_A,
  [SDLK_b] = CTI__KEY_B,
  [SDLK_c] = CTI__KEY_C,
  [SDLK_d] = CTI__KEY_D,
  [SDLK_e] = CTI__KEY_E,
  [SDLK_f] = CTI__KEY_F,
  [SDLK_g] = CTI__KEY_G,
  [SDLK_h] = CTI__KEY_H,
  [SDLK_i] = CTI__KEY_I,
  [SDLK_j] = CTI__KEY_J,
  [SDLK_k] = CTI__KEY_K,
  [SDLK_l] = CTI__KEY_L,
  [SDLK_m] = CTI__KEY_M,
  [SDLK_n] = CTI__KEY_N,
  [SDLK_o] = CTI__KEY_O,
  [SDLK_p] = CTI__KEY_P,
  [SDLK_q] = CTI__KEY_Q,
  [SDLK_r] = CTI__KEY_R,
  [SDLK_s] = CTI__KEY_S,
  [SDLK_t] = CTI__KEY_T,
  [SDLK_u] = CTI__KEY_U,
  [SDLK_v] = CTI__KEY_V,
  [SDLK_w] = CTI__KEY_W,
  [SDLK_x] = CTI__KEY_X,
  [SDLK_y] = CTI__KEY_Y,
  [SDLK_z] = CTI__KEY_Z,
  [SDLK_PLUS] = CTI__KEY_PLUS,
  [SDLK_MINUS] = CTI__KEY_MINUS,
  [SDLK_EQUALS] = CTI__KEY_EQUALS,
  [SDLK_CARET] = CTI__KEY_CARET,
  [SDLK_SPACE] = CTI__KEY_SPACE,
};


static void Config_handler(Instance *pi, void *data);
static void YUV420P_handler(Instance *pi, void *data);
static void YUV422P_handler(Instance *pi, void *data);
static void RGB3_handler(Instance *pi, void *data);
static void BGR3_handler(Instance *pi, void *data);
static void GRAY_handler(Instance *pi, void *data);
static void Keycode_handler(Instance *pi, void *data);

enum { INPUT_CONFIG, INPUT_YUV420P, INPUT_YUV422P, INPUT_RGB3, INPUT_BGR3, INPUT_GRAY, INPUT_KEYCODE };
static Input SDLstuff_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .handler = YUV420P_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = YUV422P_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = RGB3_handler },
  [ INPUT_BGR3 ] = { .type_label = "BGR3_buffer", .handler = BGR3_handler },
  [ INPUT_GRAY ] = { .type_label = "GRAY_buffer", .handler = GRAY_handler },
  [ INPUT_KEYCODE ] = { .type_label = "Keycode_msg", .handler = Keycode_handler },
};

enum { OUTPUT_FEEDBACK, OUTPUT_CONFIG, OUTPUT_KEYCODE, OUTPUT_KEYCODE_2, OUTPUT_POINTER,
       OUTPUT_YUV420P, OUTPUT_YUV422P };
static Output SDLstuff_outputs[] = {
  [ OUTPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .destination = 0L },
  [ OUTPUT_CONFIG ] = { .type_label = "Config_msg", .destination = 0L },
  [ OUTPUT_KEYCODE ] = { .type_label = "Keycode_msg", .destination = 0L },
  [ OUTPUT_KEYCODE_2 ] = { .type_label = "Keycode_msg_2", .destination = 0L },
  [ OUTPUT_POINTER ] = { .type_label = "Pointer_event", .destination = 0L },
  [ OUTPUT_YUV422P ] = {.type_label = "YUV422P_buffer", .destination = 0L },
  [ OUTPUT_YUV420P ] = {.type_label = "YUV420P_buffer", .destination = 0L },
};

enum { RENDER_MODE_GL, RENDER_MODE_OVERLAY, RENDER_MODE_SOFTWARE };

typedef struct {
  Instance i;
  int initialized;
  int renderMode;
  int videoOk;
  int inFrames;
  int vsync;
  int toggle_fullscreen;
  int fullscreen;

  double tv_sleep_until;
  double tv_last;
  double frame_last;

  SDL_Surface *surface;
  int width;
  int height;

  /* Match overlay size to screen aspect ratio. */
  int screen_aspect;
  int padLeft;
  int padRight;

  /* For resize */
  int new_width;
  int new_height;

  /* For fullscreen toggle */
  int save_width;
  int save_height;

  String label;
  int label_set;

  int rec_key;
  int snapshot_key;

  int snapshot;

  /* Overlay, used by XVideo under Linux. */
  SDL_Overlay *overlay;

  /* GL stuff. */
  struct {
    int ortho;
    int wireframe;
    int fov;
  } GL;

  /* Smoother */
  VSmoother *smoother;

  /* Filter value(s) */
  float iir_factor;

} SDLstuff_private;

static void _reset_video(SDLstuff_private *priv, const char *func);
#define reset_video(priv) _reset_video(priv, __func__);

static void update_display_times()
{
#ifdef CLOCK_MONOTONIC_RAW
  /* CLOCK_MONOTONIC_RAW is only present in linux-2.6.28+ */
  static double field_times[600];
  static int tcount = 0;

  if (tcount == 600) {
    return;
  }

  struct timespec cnow;
  clock_gettime(CLOCK_MONOTONIC_RAW, &cnow);
  field_times[tcount] = cnow.tv_sec + (cnow.tv_nsec/1000000000.0);

  tcount++;

  if (tcount == 600) {
    int i;
    FILE *f = fopen("tcount.csv", "w");
    if (f) {
      for (i=1; i < 600; i++) {
        double t0 = field_times[i-1];
        double t1 = field_times[i];
        fprintf(f, "%.9f, %.9f\n", t0, (t1-t0));
      }
      fclose(f);
      printf("field times saved\n");
    }
  }
#endif
}

static void Keycode_handler(Instance *pi, void *msg)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  Keycode_message *km = msg;
  int handled = 0;

  //   puts(__func__);

  if (km->keycode == CTI__KEY_F) {
    priv->toggle_fullscreen = 1;
    handled = 1;
  }
  else if (km->keycode == CTI__KEY_PLUS || km->keycode == CTI__KEY_EQUALS) {
    if (priv->renderMode == RENDER_MODE_OVERLAY
        && priv->width < 1440) {
      priv->new_width = priv->width * 4 / 3;
      priv->new_height = priv->height * 4 / 3;
    }
    handled = 1;
  }

  else if (km->keycode == CTI__KEY_MINUS) {
    if (priv->renderMode == RENDER_MODE_OVERLAY
        && priv->width > 320) {
      priv->new_width = priv->width * 3 / 4;
      priv->new_height = priv->height * 3 / 4;
    }
    handled = 1;
  }

  else if (km->keycode == CTI__KEY_Q) {
    handled = 1;
    exit(0);
  }

  else if (km->keycode == CTI__KEY_Z) {
    handled = 1;
    exit(1);
  }

  else if (km->keycode == priv->rec_key) {
    printf("REC\n");
  }

  else if (km->keycode == priv->snapshot_key) {
    printf("SNAPSHOT\n");
    priv->snapshot = 1;
  }


  if (!handled && pi->outputs[OUTPUT_KEYCODE_2].destination) {
    /* Pass along... */
    PostData(km, pi->outputs[OUTPUT_KEYCODE_2].destination);
  }
  else {
    Keycode_message_cleanup(&km);
  }
}


static int set_label(Instance *pi, const char *value)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;

  String_set_local(&priv->label, value);
  printf("*** label set to %s\n", sl(priv->label));
  priv->label_set = 0;
  return 0;
}

static int set_mode(Instance *pi, const char *value)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  int oldMode = priv->renderMode;

  printf("%s: setting mode to %s\n", __FILE__, value);

  if (streq(value, "GL")) {
    priv->renderMode = RENDER_MODE_GL;
  }
  else if (streq(value, "OVERLAY")) {
    priv->renderMode = RENDER_MODE_OVERLAY;
  }
  else if (streq(value, "SOFTWARE")) {
    priv->renderMode = RENDER_MODE_SOFTWARE;
  }

  if (oldMode != priv->renderMode) {
    reset_video(priv);
  }

  return 0;
}


static int set_width(Instance *pi, const char *value)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  int newWidth = atoi(value);

  if (newWidth > 0) {
    priv->new_width = newWidth;
  }

  return 0;
}


static int set_height(Instance *pi, const char *value)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  int newHeight = atoi(value);

  if (newHeight > 0) {
    priv->new_height = newHeight;
  }

  return 0;
}


static int set_fullscreen(Instance *pi, const char *value)
{
  /* Set this before video setup. */
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  priv->fullscreen = atoi(value);
  return 0;
}


static int set_smoother(Instance *pi, const char *value)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  Instance * i = InstanceGroup_find(gig, S((char*)value));
  if (i && streq(i->label, "VSmoother")) {
    priv->smoother = (VSmoother *)i;
  }
  return 0;
}


static int set_rec_key(Instance *pi, const char *value)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  priv->rec_key = Keycode_from_string(value);
  printf("set_rec_key(%s), rec_key=%d\n", value, priv->rec_key);
  return 0;
}


static int set_snapshot_key(Instance *pi, const char *value)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  priv->snapshot_key = Keycode_from_string(value);
  printf("set_snapshot_key(%s), snapshot_key=%d\n", value, priv->snapshot_key);
  return 0;
}

static int set_iir_factor(Instance *pi, const char *value)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  float factor = atof(value);
  if (0.01 < factor && factor < 0.99) {
    priv->iir_factor = atof(value);
  }
  else {
    fprintf(stderr, "SDL filter iir factor out of range\n");
  }
  return 0;
}

static int set_screen_aspect(Instance *pi, const char *value)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  priv->screen_aspect = atoi(value);
  return 0;
}


static Config config_table[] = {
  { "label", set_label, 0L, 0L},
  { "mode", set_mode, 0L, 0L},
  { "width", set_width, 0L, 0L},
  { "height", set_height, 0L, 0L},
  { "screen_aspect", set_screen_aspect, 0L, 0L},
  { "fullscreen", set_fullscreen, 0L, 0L},
  { "smoother", set_smoother, 0L, 0L},
  { "rec_key", set_rec_key, 0L, 0L},
  { "snapshot_key", set_snapshot_key, 0L, 0L},
  { "iir_factor", set_iir_factor, 0L, 0L},
};


static void _gl_setup(SDLstuff_private *priv)
{
  const uint8_t *_e = glGetString(GL_EXTENSIONS);
  char *extensions = 0L;
  if (_e) {
    extensions = strdup((const char *)_e);
    int i;
    for(i=0; extensions[i]; i++) {
      if (extensions[i] == ' ') {
        extensions[i] = '\n';
      }
    }

    {
      FILE *f = fopen("extensions.txt", "w");
      fputs(extensions, f);
      fclose(f);
    }
  }

  glViewport(0, 0, priv->width, priv->height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  //gluOrtho2D(0.0f, (GLfloat) priv->width, 0.0f, (GLfloat) priv->height);
  //gluOrtho2D(0.0f, (GLfloat) priv->width, (GLfloat) priv->height, 0.0f);
  glOrtho(0.0f, (GLfloat) priv->width, 0.0f, (GLfloat) priv->height, -1.0, 1.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

static void _reset_video(SDLstuff_private *priv, const char *func)
{
  /* See notes about this   */
  int rc;
  Uint32 sdl_vid_flags = 0;

  SDL_putenv("SDL_VIDEO_CENTERED=center");

  if (priv->fullscreen) {
    sdl_vid_flags |= SDL_FULLSCREEN;
    sdl_vid_flags |= SDL_NOFRAME;
  }
  else {
    sdl_vid_flags &= ~SDL_FULLSCREEN;
    sdl_vid_flags &= ~SDL_NOFRAME;
  }

  if (priv->renderMode == RENDER_MODE_GL) {
    rc = SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    printf("set SDL_GL_DOUBLEBUFFER returns %d\n", rc);

    if (priv->vsync) {
      rc = SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
      printf("set SDL_GL_SWAP_CONTROL returns %d\n", rc);
    }

    sdl_vid_flags |= SDL_OPENGL;
    priv->surface = SDL_SetVideoMode(priv->width, priv->height, 32,
                                    sdl_vid_flags
                                    );
  }

  else if (priv->renderMode == RENDER_MODE_OVERLAY) {
    sdl_vid_flags |= SDL_HWSURFACE | SDL_HWPALETTE | SDL_ANYFORMAT;
    priv->surface= SDL_SetVideoMode(priv->width, priv->height, 0,
                                    sdl_vid_flags
                                    );

    printf("[%s] reset_video(%d, %d)\n", func, priv->width, priv->height);
  }

  else  {
    priv->surface= SDL_SetVideoMode(priv->width, priv->height, 24,
                                    sdl_vid_flags
                                    );
  }

  if (!priv->surface) {
    printf("SetVideoMode failed!\n");
    return;
  }

  if (priv->renderMode == RENDER_MODE_GL) {
    /* Check if got wanted attributes. */
    int v;
    rc = SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &v);
    if (0 == rc) {
      printf("SDL_GL_DOUBLEBUFFER=%d\n", v);
    }

    rc = SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &v);
    if (0 == rc) {
      printf("SDL_GL_SWAP_CONTROL=%d\n", v);
    }
    _gl_setup(priv);
  }

  if (priv->overlay) {
    /* Delete the overlay, it will get recreated if needed. */
    SDL_FreeYUVOverlay(priv->overlay);
    priv->overlay = 0L;
  }

  priv->videoOk = 1;
}


static void _sdl_init(SDLstuff_private *priv)
{
  int rc;

  if (getenv("DISPLAY") == NULL) {
    fprintf(stderr, "DISPLAY not set, skipping SDL_Init()\n");
    return;
  }

  rc = SDL_Init(SDL_INIT_VIDEO);
  printf("SDL_INIT_VIDEO returns %d\n", rc);

  const SDL_VideoInfo *vi = SDL_GetVideoInfo();
  if (!vi) {
    return;
  }

  printf("%dKB video memory\n", vi->video_mem);
  printf("hardware surfaces: %s\n", vi->hw_available ? "yes" : "no");

  reset_video(priv);
}


static void render_frame_gl(SDLstuff_private *priv, RGB3_buffer *rgb3_in)
{
  glClearColor(0.0f, 0.0f, 1.0f, 0.0f);

  glClear(GL_COLOR_BUFFER_BIT);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  if (0) {
    double tv;
    cti_getdoubletime(&tv);
    printf("sdl %.6f\n", tv);
  }

  /* In OpenGL coordinates, Y moves "up".  Set raster pos, and use zoom to flip data vertically.  */
  /* NOTE: Adjust these accordingly if window is resized. */
  /* On butterfly, and maybe aria back when it was using the Intel
     driver, without the "-1" it renders *nothing* except for the
     clear color as set above. */
  //glRasterPos2i(0, priv->height);
  //glRasterPos2i(0, priv->height-1);
  glRasterPos2f(0.0, priv->height-0.0001);

  RGB3_buffer *rgb_final;

  rgb_final = rgb3_in;

  /*
   * Interestingly, the order of calling glPixelZoom and glDrawPixels doesn't seem to matter.
   * Maybe its just setting up parts of the GL pipeline before it runs.
   */
  glDrawPixels(rgb_final->width, rgb_final->height, GL_RGB, GL_UNSIGNED_BYTE, rgb_final->data);

  /* Zoom parameters should fit rgb_final image to window. */
  glPixelZoom(1.0*priv->width/rgb_final->width, -1.0*priv->height/rgb_final->height);

  /* swapBuffers should do an implicit flush, so no need to call glFlush().
     But glFinish helps a lot on software renderer (butterfly) */
  glFinish();
  SDL_GL_SwapBuffers();
}


static void filter_iir(SDLstuff_private * priv, YUV420P_buffer *yuv420p_in)
{
  static uint8_t * ysave;
  int ysize = yuv420p_in->width * yuv420p_in->width;
  int i;

  if (!ysave) {
    ysave = malloc(ysize);
    memcpy(ysave, yuv420p_in->y, ysize);
  }

  uint8_t * yout = yuv420p_in->y;

  for (i=0; i < ysize; i++) {
    yout[i]= (yout[i] * priv->iir_factor) + (ysave[i] * (1.0 - priv->iir_factor));
  }
  memcpy(ysave, yout, ysize);
}


static void render_frame_overlay(SDLstuff_private *priv, YUV420P_buffer *yuv420p_in)
{
  if (!priv->overlay) {
    int i;

    /* FIXME: different video cards have many different overlay formats, some conversions
       (like 422->420) might be avoidable. */
    int overlayWidth;
    int overlayHeight = yuv420p_in->height;

    if (priv->fullscreen
        && priv->screen_aspect
        && (global.display.width != 0)
        && (global.display.height != 0)) {
      overlayWidth = yuv420p_in->height * (global.display.width/(global.display.height*1.0));
      while (overlayWidth % 4 != 0) {
        /* Overlay should be a multiple of 4 to avoid green edge. */
        overlayWidth += 1;
      }
      priv->padLeft = (overlayWidth - yuv420p_in->width)/2;
      priv->padRight = overlayWidth - yuv420p_in->width - priv->padLeft;
    }
    else {
      overlayWidth = yuv420p_in->width;
      priv->padLeft = priv->padRight = 0;
    }

    priv->overlay = SDL_CreateYUVOverlay(overlayWidth, overlayHeight,
                                         SDL_YV12_OVERLAY,
                                         // SDL_IYUV_OVERLAY,
                                         priv->surface);
    if (!priv->overlay) {
      fprintf(stderr, "SDL_CreateYUVOverlay: SDL_error %s\n", SDL_GetError());
      exit(1);
    }

    if (priv->overlay->planes != 3) {
      fprintf(stderr, "gah, only got %d planes in overlay, wanted 3\n", priv->overlay->planes);
      exit(1);
    }

    fprintf(stderr,
            "Overlay:\n"
            "  %d, %d\n"
            "  %d planes\n"
            "  %s\n"
            "", priv->overlay->w, priv->overlay->h, priv->overlay->planes,
            priv->overlay->hw_overlay ? "hardware accelerated":"*** software only!");
    for (i=0; i < priv->overlay->planes; i++) {
      fprintf(stderr, "  %d:%p\n", priv->overlay->pitches[i], priv->overlay->pixels[i]);
    }

    fprintf(stderr, "  priv size: %d,%d\n", priv->width, priv->height);
    fprintf(stderr, "  yuv420p_in: %d,%d,%d\n", yuv420p_in->y_length, yuv420p_in->cb_length, yuv420p_in->cr_length);

  }

  int iy, next_iy; /* Start line, to allow rendering interlace fields. */
  int dy;          /* Line increment. */
  int n;      /* Number of passes, to allow for rendering 2 fields. */

  switch (yuv420p_in->c.interlace_mode) {
  case IMAGE_INTERLACE_NONE:
  default:
    iy = 0;
    next_iy = 0;
    dy = 1;
    n = 1;
    break;
  case IMAGE_INTERLACE_TOP_FIRST:
    iy = 0;
    next_iy = 1;
    dy = 2;
    n = 2;
    break;
  case IMAGE_INTERLACE_BOTTOM_FIRST:
    iy = 1;
    next_iy = 0;
    dy = 2;
    n = 2;
    break;
  }

  while (n) {
    SDL_LockSurface(priv->surface);
    SDL_LockYUVOverlay(priv->overlay);

    if (priv->iir_factor != 0.0) {
      filter_iir(priv, yuv420p_in);
    }

    /* Copy Y */
    if (1) {
      /* Pitch may be different if width is not evenly divisible by 4.
         So its a bunch of smaller memcpys instead of a single bigger
         one. */
      int y;
      unsigned char *src = yuv420p_in->y + (iy * yuv420p_in->width);
      unsigned char *dst = priv->overlay->pixels[0] + (iy * priv->overlay->pitches[0]);
      // printf("Copy Y: iy=%d h=%d dy=%d src=%p dst=%p\n", iy, priv->overlay->h, dy, src, dst);
      for (y=iy; y < priv->overlay->h; y += dy) {
        memset(dst, 16, priv->padLeft);
        memcpy(dst+priv->padLeft, src, yuv420p_in->width);
        memset(dst+priv->padLeft+yuv420p_in->width, 16, priv->padRight);
        src += yuv420p_in->width * dy;
        dst += (priv->overlay->pitches[0] * dy);
      }
    }

    /* Copy Cr, Cb */
    if (1) {
      int y, src_index, dst_index, src_width, src_height, pad_left, pad_right;
      src_width = yuv420p_in->width/2;
      src_height = yuv420p_in->height/2;
      src_index = src_width * iy;
      pad_left = priv->padLeft/2;
      pad_right = priv->padRight/2;
      for (y=iy; y < src_height; y+=dy) {
        dst_index = y * priv->overlay->pitches[1];

        memset(&priv->overlay->pixels[1][dst_index], 128, pad_left);
        memcpy(&priv->overlay->pixels[1][dst_index] + pad_left,
               &yuv420p_in->cr[src_index],
               src_width);
        memset(&priv->overlay->pixels[1][dst_index]+pad_left+src_width, 128, pad_right);


        memset(&priv->overlay->pixels[2][dst_index], 128, pad_left);
        memcpy(&priv->overlay->pixels[2][dst_index] + pad_left,
               &yuv420p_in->cb[src_index],
               src_width);
        memset(&priv->overlay->pixels[2][dst_index]+pad_left+src_width, 128, pad_right);

        src_index += (src_width * dy);
      }
    }

    SDL_UnlockYUVOverlay(priv->overlay);

    SDL_Rect rect = { 0, 0, priv->width, priv->height };
    SDL_DisplayYUVOverlay(priv->overlay, &rect);

    SDL_UnlockSurface(priv->surface);

    n -= 1;
    if (1) {
      clock_nanosleep(CLOCK_MONOTONIC,
                      0,
                      &(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/70},
                      NULL);
      iy = next_iy;
    }

    update_display_times();

  }
}

static void render_frame_software(SDLstuff_private *priv, BGR3_buffer *bgr3_in)
{
  SDL_LockSurface(priv->surface);

  if (bgr3_in &&
      priv->surface->w == bgr3_in->width &&
      priv->surface->h == bgr3_in->height &&
      priv->surface->format->BitsPerPixel == 24) {
    // printf("rendering...\n");
    int img_pitch = priv->surface->w * 3;
    if (priv->surface->pitch == img_pitch) {
      memcpy(priv->surface->pixels, bgr3_in->data, bgr3_in->data_length);
    }
    else {
      /* Pitch may be different if width is not evenly divisible by 4.  */
      int y;
      unsigned char *src = bgr3_in->data;
      unsigned char *dst = priv->surface->pixels;
      for (y=0; y < priv->surface->h; y++) {
        memcpy(dst, src, img_pitch);
        src += img_pitch;
        dst += priv->surface->pitch;
      }
    }
    SDL_UpdateRect(priv->surface, 0, 0, priv->surface->w, priv->surface->h);
  }
  else {
    printf("wrong BitsPerPixel=%d\n", priv->surface->format->BitsPerPixel);
  }


  SDL_UnlockSurface(priv->surface);
}


static void pre_render_frame(SDLstuff_private *priv, int width, int height, Image_common *c)
{
  dpf("frame %d ready for display\n", priv->inFrames);
  int need_reset = 0;

  if (priv->toggle_fullscreen
      && (global.display.width != 0)
      && (global.display.height != 0)) {
    if (!priv->fullscreen) {
      priv->fullscreen = 1;
      priv->save_width = priv->width;
      priv->save_height = priv->height;
      priv->new_width = global.display.width;
      priv->new_height = global.display.height;
    }
    else {
      priv->fullscreen = 0;
      priv->new_width = priv->save_width;
      priv->new_height = priv->save_height;
    }
    priv->toggle_fullscreen = 0;
    need_reset = 1;
  }

  if (priv->new_width && priv->new_height) {
    if (priv->new_width != priv->width ||
        priv->new_height != priv->height) {
      /* Resize based on config-adjusted size. */
      priv->width = priv->new_width;
      priv->height = priv->new_height;
      need_reset = 1;
    }
  }
  else if (!priv->fullscreen
           && width != priv->width
           && height != priv->height) {
    /* Resize based on incoming frame. */
    priv->width = width;
    priv->height = height;
    need_reset = 1;
  }

  if (need_reset) {
    reset_video(priv);
  }

  if (sl(priv->label) && !priv->label_set) {
    SDL_WM_SetCaption(sl(priv->label), NULL); //Sets the Window Title
    priv->label_set = 1;
  }

  /* Handle frame delay timing. IN PROGRESS... */
  // x(c->tv, priv->frame_last);
  if (c) {
    // priv->frame_last = c->tv;
  }

  if (priv->smoother) {
    VSmoother_smooth(priv->smoother,
                     c->timestamp,
                     ((Instance *)priv)->pending_messages);
  }
}

static void post_render_frame(Instance *pi)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  if (pi->outputs[OUTPUT_FEEDBACK].destination) {
    Feedback_buffer *fb = Feedback_buffer_new();
    /* FIXME:  Maybe could get propagate sequence and pass it back here... */
    fb->seq = 0;
    PostData(fb, pi->outputs[OUTPUT_FEEDBACK].destination);
  }
  priv->inFrames += 1;
}


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void YUV420P_handler(Instance *pi, void *data)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  YUV420P_buffer *yuv420p = data;
  BGR3_buffer *bgr3 = NULL;
  RGB3_buffer *rgb3 = NULL;
  //Gray_buffer *gray = NULL;

  if (0) {
    static int first = 1;
    if (first) {
      first = 0;
      FILE *f;
      f = fopen("y.pgm", "wb");
      if (f) {
        fprintf(f, "P5\n%d %d\n255\n", yuv420p->width, yuv420p->height);
        if (fwrite(yuv420p->y, yuv420p->y_length, 1, f) != 1) { perror("fwrite"); }
        fclose(f);
      }
      f = fopen("cr.pgm", "wb");
      if (f) {
        fprintf(f, "P5\n%d %d\n255\n", yuv420p->width/2, yuv420p->height/2);
        if (fwrite(yuv420p->cr, yuv420p->cr_length, 1, f) != 1) { perror("fwrite"); }
        fclose(f);
      }
      f = fopen("cb.pgm", "wb");
      if (f) {
        fprintf(f, "P5\n%d %d\n255\n", yuv420p->width/2, yuv420p->height/2);
        if (fwrite(yuv420p->cb, yuv420p->cb_length, 1, f) != 1) { perror("fwrite"); }
        fclose(f);
      }
    }
  }

  pre_render_frame(priv, yuv420p->width, yuv420p->height, &yuv420p->c);

  switch (priv->renderMode) {
  case RENDER_MODE_GL:
    {
      rgb3 = YUV420P_to_RGB3(yuv420p);
      render_frame_gl(priv, rgb3);
      RGB3_buffer_release(rgb3);
    }
    break;
  case RENDER_MODE_OVERLAY:
    {
      render_frame_overlay(priv, yuv420p);
    }
    break;
  case RENDER_MODE_SOFTWARE:
    {
      bgr3 = YUV420P_to_BGR3(yuv420p);
      render_frame_software(priv, bgr3);
    }
    break;
  }

  post_render_frame(pi);


  if (rgb3) {
    RGB3_buffer_release(rgb3);
  }

  if (yuv420p) {
    YUV420P_buffer_release(yuv420p);
  }

  if (bgr3) {
    BGR3_buffer_release(bgr3);
  }
}



static void YUV422P_handler(Instance *pi, void *data)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  YUV422P_buffer *yuv422p = data;
  YUV420P_buffer *yuv420p = NULL;
  BGR3_buffer *bgr3 = NULL;
  RGB3_buffer *rgb3 = NULL;
  // Gray_buffer *gray = NULL;

  pre_render_frame(priv, yuv422p->width, yuv422p->height, &yuv422p->c);

  switch (priv->renderMode) {
  case RENDER_MODE_GL:
    {
      rgb3 = YUV422P_to_RGB3(yuv422p);
      render_frame_gl(priv, rgb3);
    }
    break;
  case RENDER_MODE_OVERLAY:
    {
      yuv420p = YUV422P_to_YUV420P(yuv422p);
      render_frame_overlay(priv, yuv420p);
    }
    break;
  case RENDER_MODE_SOFTWARE:
    {
      bgr3 = YUV422P_to_BGR3(yuv422p);
      render_frame_software(priv, bgr3);
    }
    break;
  }

  post_render_frame(pi);

  if (bgr3) {
    BGR3_buffer_release(bgr3);
  }

  if (rgb3) {
    RGB3_buffer_release(rgb3);
  }

  if (yuv420p) {
    YUV420P_buffer_release(yuv420p);
  }

  if (yuv422p) {
    if (priv->snapshot && pi->outputs[OUTPUT_YUV422P].destination) {
      PostData(YUV422P_buffer_ref(yuv422p), pi->outputs[OUTPUT_YUV422P].destination);
      priv->snapshot = 0;
    }
    YUV422P_buffer_release(yuv422p);
  }

}


static void RGB3_handler(Instance *pi, void *data)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  RGB3_buffer *rgb3 = data;
  YUV420P_buffer *yuv420p = NULL;
  BGR3_buffer *bgr3 = NULL;
  //Gray_buffer *gray = NULL;

  if (0) {
    static int n = 0;
    n += 1;
    if (n == 10) {
      printf("Saving frame %d as ppm\n", n);
      FILE *f;
      f = fopen("x.ppm", "wb");
      if (f) {
        fprintf(f, "P6\n%d %d\n255\n", rgb3->width, rgb3->height);
        if (fwrite(rgb3->data, rgb3->width * rgb3->height *3, 1, f) != 1) { perror("fwrite"); }
        fclose(f);
      }
    }
  }

  pre_render_frame(priv, rgb3->width, rgb3->height, &rgb3->c);

  switch (priv->renderMode) {
  case RENDER_MODE_GL:
    {
      render_frame_gl(priv, rgb3);
    }
    break;
  case RENDER_MODE_OVERLAY:
    {
      yuv420p = RGB3_to_YUV420P(rgb3);
      render_frame_overlay(priv, yuv420p);
    }
    break;
  case RENDER_MODE_SOFTWARE:
    {
      rgb3_to_bgr3(&rgb3, &bgr3);
      render_frame_software(priv, bgr3);
    }
    break;
  }

  post_render_frame(pi);

  if (rgb3) {
    RGB3_buffer_release(rgb3);
  }

  if (yuv420p) {
    YUV420P_buffer_release(yuv420p);
  }

  if (bgr3) {
    BGR3_buffer_release(bgr3);
  }

}


static void BGR3_handler(Instance *pi, void *data)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  BGR3_buffer *bgr3 = data;
  RGB3_buffer *rgb3 = NULL;
  YUV420P_buffer *yuv420p = NULL;
  //Gray_buffer *gray = NULL;

  pre_render_frame(priv, bgr3->width, bgr3->height, &bgr3->c);
  switch (priv->renderMode) {
  case RENDER_MODE_GL:
    {
      /* FIXME: glDrawPixels can handle GL_BGR, so could just pass that along...*/
      bgr3_to_rgb3(&bgr3, &rgb3);
      render_frame_gl(priv, rgb3);
    }
    break;
  case RENDER_MODE_OVERLAY:
    {
      /* FIXME... */
      // render_frame_overlay(priv, yuv420p);
    }
    break;
  case RENDER_MODE_SOFTWARE:
    {
      render_frame_software(priv, bgr3);
    }
    break;
  }
  post_render_frame(pi);

  if (rgb3) {
    RGB3_buffer_release(rgb3);
  }

  if (yuv420p) {
    YUV420P_buffer_release(yuv420p);
  }

  if (bgr3) {
    BGR3_buffer_release(bgr3);
  }
}


static void GRAY_handler(Instance *pi, void *data)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  Gray_buffer *gray = data;
  BGR3_buffer *bgr3 = NULL;
  RGB3_buffer *rgb3 = NULL;
  YUV420P_buffer *yuv420p = NULL;

  pre_render_frame(priv, gray->width, gray->height, &gray->c);
  switch (priv->renderMode) {
  case RENDER_MODE_GL:
    {
      rgb3 = RGB3_buffer_new(gray->width, gray->height, &gray->c);
      int i;
      int j = 0;
      /* FIXME: Do this efficiently...*/
      for (i=0; i < gray->width*gray->height; i++) {
        rgb3->data[j] = rgb3->data[j+1] = rgb3->data[j+2] = gray->data[i];
        j += 3;
      }
      render_frame_gl(priv, rgb3);
    }
    break;
  case RENDER_MODE_OVERLAY:
    {
      yuv420p = YUV420P_buffer_new(gray->width, gray->height, &gray->c);
      memcpy(yuv420p->y, gray->data, gray->data_length);
      memset(yuv420p->cb, 128, yuv420p->cb_length);
      memset(yuv420p->cr, 128, yuv420p->cr_length);
      render_frame_overlay(priv, yuv420p);
    }
    break;
  case RENDER_MODE_SOFTWARE:
    {
      bgr3 = BGR3_buffer_new(gray->width, gray->height, &gray->c);
      int i;
      int j = 0;
      /* FIXME: Do this efficiently...*/
      for (i=0; i < gray->width*gray->height; i++) {
        bgr3->data[j] = bgr3->data[j+1] = bgr3->data[j+2] = gray->data[i];
        j += 3;
      }
      render_frame_software(priv, bgr3);
    }
    break;
  }
  post_render_frame(pi);

  if (gray) {
    Gray_buffer_release(gray);
  }

  if (rgb3) {
    RGB3_buffer_release(rgb3);
  }

  if (yuv420p) {
    if (priv->snapshot && pi->outputs[OUTPUT_YUV420P].destination) {
      PostData(YUV420P_buffer_ref(yuv420p), pi->outputs[OUTPUT_YUV420P].destination);
      priv->snapshot = 0;
    }
    YUV420P_buffer_release(yuv420p);
  }

  if (bgr3) {
    BGR3_buffer_release(bgr3);
  }
}


Instance *my_instance;

void sdl_event_loop(void)
{
  /* This needs to run in the context of the main application thread, for
     certain platforms (OSX in particular). */
  Handler_message *hm;
  SDL_Event ev;
  int rc;
  Instance *pi = my_instance;

  CTI_register_instance(pi);    /* Required for stack debugging, since not using
                                   Instance_thread_main(). */

  SDLstuff_private *priv = (SDLstuff_private *)pi;

  priv->width = 640;
  priv->height = 480;
  priv->GL.fov = 90;
  priv->vsync = 1;
  //priv->renderMode = RENDER_MODE_GL;
  //priv->renderMode = RENDER_MODE_SOFTWARE;
  priv->renderMode = RENDER_MODE_OVERLAY;


  printf("%s started\n", __func__);

  /* NOTE: For compatibility with certain platforms, the thread that
     calls SDL_WaitEvent() must be the same thread that sets up the
     video.  Both happen in this function. */

  _sdl_init(priv);

  while (1) {
    rc = SDL_WaitEvent(&ev);

    // printf("ev.type=%d\n", ev.type);

    if (!rc) {
      fprintf(stderr, "SDL_WaitEvent: SDL_error %d %s\n", rc, SDL_GetError());
      exit(1);
      continue;
    }
    if (ev.type == SDL_USEREVENT) {
      hm = ev.user.data1;
      hm->handler(pi, hm->data);
      ReleaseMessage(&hm,pi);
    }
    else if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
      if (pi->outputs[OUTPUT_POINTER].destination) {
        Pointer_event *p = Pointer_event_new(ev.button.x,ev.button.y,
                                             ev.button.button, ev.button.state);
        PostData(p, pi->outputs[OUTPUT_POINTER].destination);
      }
    }
    else if (ev.type == SDL_KEYUP) {
      /* Handle ranges here. */
      if (pi->outputs[OUTPUT_KEYCODE].destination) {
        PostData(Keycode_message_new(SDLtoCTI_Keymap[ev.key.keysym.sym]),
                 pi->outputs[OUTPUT_KEYCODE].destination);
      }
    }
    else if (ev.type == SDL_QUIT) {
      fprintf(stderr, "Got SDL_QUIT\n");
      exit(0);
    }
  }
}


static void SDLstuff_tick(Instance *pi)
{
  /* This is called from the "SDLstuff" instance thread.  Config
     messages are passed to the main thread. */
  Handler_message *hm;

  hm = GetData(pi, 1);

  if (hm && hm->handler == Config_handler) {
    printf("%s got a config message\n", __func__);
  }

  if (hm) {
    SDL_Event ev = {  .user.type = SDL_USEREVENT,
                      .user.code = 0,
                      .user.data1 = hm,
                      .user.data2 = 0L
    };

    /* PushEvent will fail until SDL is initialized in the other thread. */
    while (SDL_PushEvent(&ev) == -1) {
      // printf("could not push event, retrying in 100ms...\n");
      SDL_Delay(100);
    }
  }
}


#ifdef __APPLE__
/* Special version in SDLMain.m that does OSX required setup, then
   calls back to sdl_event_loop(). */
extern int platform_sdl_main(int argc, char *argv[]);
#else
int platform_sdl_main(int argc, char *argv[])
{
  sdl_event_loop();
  return 0;
}
#endif


static void SDLstuff_instance_init(Instance *pi)
{
  SDLstuff_private *priv = (SDLstuff_private *)pi;
  priv->rec_key = -1;
  priv->snapshot_key = -1;
  priv->iir_factor = 0.0;

#if 0
  extern Callback *ui_callback; /* cti_app.c */
  Callback_fill(ui_callback, my_event_loop, pi);
#endif

  my_instance = pi;
  ui_main = platform_sdl_main;
}


static Template SDLstuff_template = {
  .label = "SDLstuff",
  .priv_size = sizeof(SDLstuff_private),
  .inputs = SDLstuff_inputs,
  .num_inputs = table_size(SDLstuff_inputs),
  .outputs = SDLstuff_outputs,
  .num_outputs = table_size(SDLstuff_outputs),
  .tick = SDLstuff_tick,
  .instance_init = SDLstuff_instance_init,
};

void SDLstuff_init(void)
{
  Template_register(&SDLstuff_template);
}
