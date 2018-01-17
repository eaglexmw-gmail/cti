/* Serial port module */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <sys/types.h>          /* open */
#include <sys/stat.h>           /* open */
#include <fcntl.h>              /* open */
#include <unistd.h>             /* close */
#include <termios.h>

#include "CTI.h"
#include "SerialIO.h"
#include "SourceSink.h"
#include "String.h"
#include "localptr.h"

static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input SerialIO_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
};

//enum { /* OUTPUT_... */ };
static Output SerialIO_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};

typedef struct {
  Instance i;
  int fd;
  int baud;
  struct termios saved_settings;  
  struct termios new_settings;
} SerialIO_private;

typedef struct {} CommandContext;
extern void Command_add(const char * templateName, const char * command,
                        void (*func)(Instance *pi, CommandContext * cctx));
extern String * CommandContext_get_string(CommandContext * cctx, String * key);
extern int CommandContext_get_int(CommandContext * cctx, String * key);
extern String_list * Glob(String * pattern);

static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};

static void SerialIO_open(Instance *pi, CommandContext * cctx)
{
  int i;
  SerialIO_private *priv = (SerialIO_private*)pi;
  if (priv->fd != -1) {
    close(priv->fd);
  }
  int rate = CommandContext_get_int(cctx, S("rate"));
  switch (rate) {
  case 9600: priv->baud = B9600; break;
  case 19200: priv->baud = B19200; break;
  case 38400: priv->baud = B38400; break;
  case 57600: priv->baud = B57600; break;
  case 115200: priv->baud = B115200; break;
  default: puts("Invalid baud specified."); return;
  }
    
  localptr(String, devpattern) = CommandContext_get_string(cctx, S("device"));
  if (String_is_none(devpattern)) { return; }
  
  localptr(String_list, devices) = Glob(devpattern);
  for (i=0; i < String_list_len(devices); i++) {
    localptr(String, device) = String_list_get(devices, i);
    priv->fd = open(s(device), O_RDWR);
    if (priv->fd != -1) {
      break;
    }
  }
  
  if (priv->fd == -1) { return; }

  if (tcgetattr(priv->fd, &priv->saved_settings) != 0) { perror("tcgetattr"); }
  priv->new_settings = priv->saved_settings;
  cfsetospeed(&priv->new_settings, priv->baud);
  cfsetispeed(&priv->new_settings, priv->baud);
  priv->new_settings.c_cflag &= ~(CSTOPB | PARENB | PARODD | HUPCL | CRTSCTS);
  if (tcsetattr(priv->fd, TCSANOW, &priv->new_settings) != 0) { perror("tcsetattr"); }
  
}


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void SerialIO_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void SerialIO_instance_init(Instance *pi)
{
  // SerialIO_private *priv = (SerialIO_private *)pi;
}


static Template SerialIO_template = {
  .label = "SerialIO",
  .priv_size = sizeof(SerialIO_private),  
  .inputs = SerialIO_inputs,
  .num_inputs = table_size(SerialIO_inputs),
  .outputs = SerialIO_outputs,
  .num_outputs = table_size(SerialIO_outputs),
  .tick = SerialIO_tick,
  .instance_init = SerialIO_instance_init,
};

void SerialIO_init(void)
{
  Template_register(&SerialIO_template);
  Command_add("SerialIO", "open", SerialIO_open);
  //Command_add("SerialIO", "period", SerialIO_period);
  //Command_add("SerialIO", "interact", SerialIO_interact);
}
