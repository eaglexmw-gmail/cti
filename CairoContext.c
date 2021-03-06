#include <stdio.h>              /* fprintf */
#include <stdlib.h>             /* calloc */
#include <string.h>             /* memcpy */
#include <time.h>               /* localtime */
#include <math.h>               /* M_2_PI */

#include "CTI.h"
#include "CairoContext.h"
#include "Images.h"
#include "Array.h"
#include "File.h"
#include "localptr.h"

#include "cairo.h"

static void Config_handler(Instance *pi, void *msg);
static void Command_handler(Instance *pi, void *msg);
static void rgb3_handler(Instance *pi, void *msg);
static void y422p_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_COMMAND, INPUT_RGB3, INPUT_YUV422P };
static Input CairoContext_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_COMMAND ] = { .type_label = "CairoCommand", .handler = Command_handler },
  [ INPUT_RGB3 ] = { .type_label = "RGB3_buffer", .handler = rgb3_handler },
  [ INPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .handler = y422p_handler },
};

enum { OUTPUT_RGB3, OUTPUT_YUV422P,  OUTPUT_YUV420P };
static Output CairoContext_outputs[] = {
  [ OUTPUT_RGB3 ] = { .type_label = "RGB3_buffer", .destination = 0L },
  [ OUTPUT_YUV422P ] = { .type_label = "YUV422P_buffer", .destination = 0L },
  [ OUTPUT_YUV420P ] = { .type_label = "YUV420P_buffer", .destination = 0L },
};


/* Map text strings to commands. */
static struct {
  const char *label;
  int command;
  int num_double_args;
} CairoCommandMap [] = {
  { .label = "set_source_rgb", .command = CC_COMMAND_SET_SOURCE_RGB, .num_double_args = 3 },
  { .label = "set_source_rgba", .command = CC_COMMAND_SET_SOURCE_RGB, .num_double_args = 4 },
  { .label = "set_line_width", .command = CC_COMMAND_SET_LINE_WIDTH, .num_double_args = 1 },

  { .label = "move_to", .command = CC_COMMAND_MOVE_TO, .num_double_args = 2 },
  { .label = "line_to", .command = CC_COMMAND_LINE_TO, .num_double_args = 2 },
  { .label = "rel_move_to", .command = CC_COMMAND_REL_MOVE_TO, .num_double_args = 2 },
  { .label = "rel_line_to", .command = CC_COMMAND_REL_LINE_TO, .num_double_args = 2 },
  { .label = "close_path", .command = CC_COMMAND_CLOSE_PATH, .num_double_args = 0 },

  { .label = "translate", .command = CC_COMMAND_TRANSLATE, .num_double_args = 2 },
  { .label = "scale", .command = CC_COMMAND_SCALE, .num_double_args = 2 },
  { .label = "rotate", .command = CC_COMMAND_ROTATE, .num_double_args = 1 },

  { .label = "fill", .command = CC_COMMAND_FILL, .num_double_args = 0 },
  { .label = "stroke", .command = CC_COMMAND_STROKE, .num_double_args = 0 },

  { .label = "push_group", .command = CC_COMMAND_PUSH_GROUP, .num_double_args = 0 },
  { .label = "pop_group", .command = CC_COMMAND_POP_GROUP, .num_double_args = 0 },
  { .label = "identity_matrix", .command = CC_COMMAND_IDENTITY_MATRIX, .num_double_args = 0 },

  { .label = "set_font_size", .command = CC_COMMAND_SET_FONT_SIZE, .num_double_args = 1 },
  { .label = "show_text", .command = CC_COMMAND_SHOW_TEXT, .num_double_args = 0 },

  { .label = "rotate_subseconds", .command = CC_COMMAND_ROTATE_SUBSECONDS, .num_double_args = 0 },
  { .label = "rotate_seconds", .command = CC_COMMAND_ROTATE_SECONDS, .num_double_args = 0 },
  { .label = "rotate_minutes", .command = CC_COMMAND_ROTATE_MINUTES, .num_double_args = 0 },
  { .label = "rotate_hours", .command = CC_COMMAND_ROTATE_HOURS, .num_double_args = 0 },
};

typedef struct {
  Instance i;
  cairo_surface_t *surface;
  cairo_t *context;
  /* FIXME: Enable left, top */
  //int left;
  //int top;
  int width;
  int height;
  Array(CairoCommand) commands;
  String * text;
  long timeout;
  long timeout_timestamp;
  String * hostname;
  String * label;
} CairoContext_private;


static int set_show_text(Instance *pi, const char *value)
{
  CairoContext_private *priv = (CairoContext_private *)pi;

  if (!String_is_none(priv->text)) {
    String_clear(&priv->text);
  }
  //priv->text = String_new(value);
  priv->text = String_unescape(SC(value));

  if (priv->timeout) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    priv->timeout_timestamp = tv.tv_sec;
  }

  return 0;
}


static int set_label(Instance *pi, const char *value)
{
  CairoContext_private *priv = (CairoContext_private *)pi;
  if (priv->label) {
    String_free(&priv->label);
  }
  priv->label = String_new(value);
  return 0;
}


static void do_system_text(CairoContext_private * priv, String * key)
{
  dpf("%s: key=%s\n", __func__, s(key));
  localptr(String, value) = String_value_none();
  if (streq(s(key), "hostname")) {
    value = String_dup(priv->hostname);
    String_trim_right(value);
  }
  else if (streq(s(key), "datetime")) {
    char datetime[128];
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (lt) {
      strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S %Z", lt);
      value = String_new(datetime);
    }
  }
  else if (streq(s(key), "wifistatus")) {
    //value = String_new("WiFi ...");
    //system("iwconfig wlan0 | grep Signal | sed -e 's/.*Signal.level=//g' -e 's/[ ].*$//g' > /dev/shm/wifi.txt");
    //localptr(String, wifitmp) = File_load_text(S("/dev/shm/wifi.txt"));
    //String_cat1(value, s(wifitmp));
    //String_trim_right(value);
  }
  else if (streq(s(key), "memstatus")) {
    //system("grep -E 'MemFree' /proc/meminfo  | sed -e 's/.kB$//g' -e 's/.* //g' > /dev/shm/memstatus.txt");
    //value = File_load_text(S("/dev/shm/memstatus.txt"));
    //String_trim_right(value);
    //String_cat1(value, "kB free");
  }
  if (!String_is_none(value)) {
    cairo_show_text(priv->context, value->bytes);
  }
}


static int add_command(Instance *pi, const char *value)
{
  CairoContext_private *priv = (CairoContext_private *)pi;
  CairoCommand cmd = {};
  char label[256] = {};
  int i;
  int j;
  int n;
  int found = 0;

  n = sscanf(value, "%255s %lf %lf %lf %lf %lf %lf",
             label,
             &cmd.args[0],
             &cmd.args[1],
             &cmd.args[2],
             &cmd.args[3],
             &cmd.args[4],
             &cmd.args[5]);

  fprintf(stderr, "add_command label=%s\n", label[0] ? label : "" );
  fprintf(stderr, "%d %d\n", streq(value, "system_text") , value[strlen(value)] != 0);

  if (streq(label, "set_text") && value[strlen(label)] != 0) {
    cmd.command = CC_COMMAND_SET_TEXT;
    cmd.text = String_new(value + strlen(label) + 1);
    Array_append(priv->commands, cmd);
    found = 1;
    goto out;
  }

  else if (streq(label, "system_text") && value[strlen(label)] != 0) {
    cmd.command = CC_COMMAND_SYSTEM_TEXT;
    cmd.text = String_new(value + strlen(label) + 1);
    Array_append(priv->commands, cmd);
    found = 1;
    goto out;
  }

  for (i=0; i < table_size(CairoCommandMap); i++) {
    if (streq(label, CairoCommandMap[i].label)
        && (n-1) == CairoCommandMap[i].num_double_args) {
      cmd.command = CairoCommandMap[i].command;
      printf("cairo_%s", label);
      for (j=0; j < CairoCommandMap[i].num_double_args; j++) {
        printf(" %lf", cmd.args[j]);
      }
      printf("\n");
      Array_append(priv->commands, cmd);
      found = 1;
      goto out;
    }
  }

  if (!found) {
    n = sscanf(value, "%255s",
               label);
    if (streq(label, "reset")) {
      Array_free(priv->commands, CairoCommand, NULL);
    }
  }

 out:
  return 0;
}

static void apply_commands(CairoContext_private *priv, RGB3_buffer * rgb3)
{
  int i;
  time_t t;
  struct tm lt;
  double rval = 0.0;

  priv->surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, priv->width, priv->height);
  priv->context = cairo_create(priv->surface);

  for (i=0; i < Array_count(priv->commands); i++) {
    CairoCommand cmd = Array_get(priv->commands, i);

    switch (cmd.command) {

    case CC_COMMAND_SET_SOURCE_RGB:
      cairo_set_source_rgb(priv->context, cmd.args[0], cmd.args[1], cmd.args[2]);
      break;

    case CC_COMMAND_SET_SOURCE_RGBA:
      cairo_set_source_rgba(priv->context, cmd.args[0], cmd.args[1], cmd.args[2], cmd.args[3]);
      break;

    case CC_COMMAND_SET_LINE_WIDTH:
      cairo_set_line_width(priv->context, cmd.args[0]);
      break;

    case CC_COMMAND_MOVE_TO:
      cairo_move_to(priv->context, cmd.args[0], cmd.args[1]);
      break;
    case CC_COMMAND_LINE_TO:
      cairo_line_to(priv->context, cmd.args[0], cmd.args[1]);
      break;
    case CC_COMMAND_REL_MOVE_TO:
      cairo_rel_move_to(priv->context, cmd.args[0], cmd.args[1]);
      break;
    case CC_COMMAND_REL_LINE_TO:
      cairo_rel_line_to(priv->context, cmd.args[0], cmd.args[1]);
      break;
    case CC_COMMAND_CLOSE_PATH:
      cairo_close_path(priv->context);
      break;

    case CC_COMMAND_TRANSLATE:
      cairo_translate(priv->context, cmd.args[0], cmd.args[1]);
      break;
    case CC_COMMAND_SCALE:
      cairo_scale(priv->context, cmd.args[0], cmd.args[1]);
      break;
    case CC_COMMAND_ROTATE:
      cairo_rotate(priv->context, cmd.args[0]);
      break;

    case CC_COMMAND_FILL:
      cairo_fill(priv->context);
      break;
    case CC_COMMAND_STROKE:
      cairo_stroke(priv->context);
      break;

    case CC_COMMAND_PUSH_GROUP:
      cairo_push_group(priv->context);
      break;
    case CC_COMMAND_POP_GROUP:
      cairo_pop_group(priv->context);
      break;
    case CC_COMMAND_IDENTITY_MATRIX:
      cairo_identity_matrix(priv->context);
      break;

    case CC_COMMAND_SET_FONT_SIZE:
      cairo_set_font_size(priv->context, cmd.args[0]);
      break;
    case CC_COMMAND_SHOW_TEXT:
      if (s(priv->text)) {
        cairo_show_text(priv->context, s(priv->text));
      }
      break;

    case CC_COMMAND_SET_TEXT:
      if (cmd.text) {
        cairo_show_text(priv->context, cmd.text->bytes);
      }
      break;
    case CC_COMMAND_SYSTEM_TEXT:
      if (cmd.text) {
        do_system_text(priv, cmd.text);
      }
      break;

    case CC_COMMAND_ROTATE_SUBSECONDS:
      rval = (2*M_PI) * fmod(rgb3->c.timestamp , 1.0);
      cairo_rotate(priv->context, rval);
      break;
    case CC_COMMAND_ROTATE_SECONDS:
      t = (time_t)rgb3->c.timestamp;
      localtime_r(&t, &lt);
      // printf("t=%ld lt.tm_sec=%d lt.tm_min=%d lt.tm_hour=%d\n", t, lt.tm_sec, lt.tm_min, lt.tm_hour);
      rval = (2*M_PI / 60) * (lt.tm_sec /* + (rgb3->c.tv.tv_usec/1000000.0) */);
      cairo_rotate(priv->context, rval);
      break;
    case CC_COMMAND_ROTATE_MINUTES:
      t = (time_t)rgb3->c.timestamp;
      localtime_r(&t, &lt);
      rval = (2*M_PI / 60) * (lt.tm_min + (lt.tm_sec/60.0));
      cairo_rotate(priv->context, rval);
      break;
    case CC_COMMAND_ROTATE_HOURS:
      t = (time_t)rgb3->c.timestamp;
      localtime_r(&t, &lt);
      rval = (2*M_PI / 12) * (lt.tm_hour + (lt.tm_min/60.0) + (lt.tm_sec/3600.0));
      cairo_rotate(priv->context, rval);
      break;

    default:
      break;
    }
  }

  dpf("%d cairo operations done, status=%d\n", i, cairo_status(priv->context));

  /* Now merge into RGB buffer. */
  RGB_buffer_merge_rgba(rgb3,
                        cairo_image_surface_get_data(priv->surface),
                        cairo_image_surface_get_width(priv->surface),
                        cairo_image_surface_get_height(priv->surface),
                        cairo_image_surface_get_stride(priv->surface));

  if (!String_is_none(priv->label) && !rgb3->c.label) {
    rgb3->c.label = String_dup(priv->label);
  }

  cairo_surface_destroy(priv->surface);
  cairo_destroy(priv->context);
}

static void rgb3_handler(Instance *pi, void *msg)
{
  CairoContext_private *priv = (CairoContext_private *)pi;
  RGB3_buffer * rgb3 = msg;
  int do_apply = 1;

  if (priv->timeout) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (priv->timeout_timestamp + priv->timeout <  tv.tv_sec) {
      do_apply = 0;
    }
  }

  if (do_apply) {
    apply_commands(priv, rgb3);
  }

  if (pi->outputs[OUTPUT_YUV420P].destination) {
    YUV420P_buffer * yuv = RGB3_to_YUV420P(rgb3);
    PostData(yuv, pi->outputs[OUTPUT_YUV420P].destination);
  }

  if (pi->outputs[OUTPUT_RGB3].destination) {
    PostData(rgb3, pi->outputs[OUTPUT_RGB3].destination);
  }
  else {
    RGB3_buffer_release(rgb3);
  }
}

static void y422p_handler(Instance *pi, void *msg)
{
  CairoContext_private *priv = (CairoContext_private *)pi;
  YUV422P_buffer * y422p = msg;
  int do_apply = 1;

  if (priv->timeout) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (priv->timeout_timestamp + priv->timeout <  tv.tv_sec) {
      do_apply = 0;
    }
  }

  if (do_apply) {
    YUV422P_buffer * temp;
    RGB3_buffer * rgb3;
    /* I could avoid some of the "temp" buffers here by setting up
       in-place structures, if strides are used correctly in the image
       code. */
    temp = YUV422P_copy(y422p, 0, 0, priv->width, priv->height);
    rgb3 = YUV422P_to_RGB3(temp);
    rgb3->c.timestamp = y422p->c.timestamp;     /* Preserve timestamp! */
    YUV422P_buffer_release(temp);
    apply_commands(priv, rgb3);
    temp = RGB3_to_YUV422P(rgb3);
    RGB3_buffer_release(rgb3);
    YUV422P_paste(y422p, temp, 0, 0, priv->width, priv->height);
    YUV422P_buffer_release(temp);
  }

  if (pi->outputs[OUTPUT_YUV422P].destination) {
    PostData(y422p, pi->outputs[OUTPUT_YUV422P].destination);
  }
  else {
    YUV422P_buffer_release(y422p);
  }
}


static Config config_table[] = {
  { "width",     0L, 0L, 0L, cti_set_int, offsetof(CairoContext_private, width) },
  { "height",    0L, 0L, 0L, cti_set_int, offsetof(CairoContext_private, height) },
  { "timeout",   0L, 0L, 0L, cti_set_int, offsetof(CairoContext_private, timeout) },
  { "command",   add_command, 0L, 0L },
  { "text",      set_show_text, 0L, 0L },
  { "label",     set_label, 0L, 0L },
};

static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}

static void Command_handler(Instance *pi, void *data)
{
  /* Binary command interface, avoids generating and parsing commands. */
}


static void CairoContext_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void CairoContext_instance_init(Instance *pi)
{
  CairoContext_private *priv = (CairoContext_private *)pi;
  priv->text = String_value_none();
  priv->label = String_value_none();
  priv->hostname = File_load_text(S("/etc/hostname"));
  String_trim_right(priv->hostname);
}


static Template CairoContext_template = {
  .label = "CairoContext",
  .priv_size = sizeof(CairoContext_private),
  .inputs = CairoContext_inputs,
  .num_inputs = table_size(CairoContext_inputs),
  .outputs = CairoContext_outputs,
  .num_outputs = table_size(CairoContext_outputs),
  .tick = CairoContext_tick,
  .instance_init = CairoContext_instance_init,
};

void CairoContext_init(void)
{
  Template_register(&CairoContext_template);
}
