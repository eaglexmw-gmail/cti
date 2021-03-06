#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#include "DJpeg.h"
#include "Images.h"
#include "CTI.h"
#include "ALSAio.h"
#include "MjpegDemux.h"
#include "SDLstuff.h"

int app_code(int argc, char *argv[])
{
  MjpegDemux_init();
  DJpeg_init();
  ALSAPlayback_init();
  SDLstuff_init();

  Instance *md = Instantiate("MjpegDemux");
  if (md) {
    printf("instance: %s\n", md->label);
  }

  Instance *ap = Instantiate("ALSAPlayback");
  if (ap) {
    printf("instance: %s\n", ap->label);
  }

  Instance *sdl = Instantiate("SDLstuff");
  if (sdl) {
    printf("instance: %s\n", sdl->label);
  }

  Instance_loop_thread(sdl);

  while (1) {
    sleep(1);
  }

  return 0;
}
