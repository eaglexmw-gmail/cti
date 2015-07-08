#ifndef _CTI_KEYCODES_H_
#define _CTI_KEYCODES_H_

/* Keycodes */
enum {
CTI__KEY_UNKNOWN,
CTI__KEY_TV,
CTI__KEY_VIDEO,
CTI__KEY_MP3,
CTI__KEY_MEDIA,
CTI__KEY_RADIO,
CTI__KEY_OK,
CTI__KEY_VOLUMEDOWN,
CTI__KEY_VOLUMEUP,
CTI__KEY_CHANNELUP,
CTI__KEY_CHANNELDOWN,
CTI__KEY_PREVIOUS,
CTI__KEY_MUTE,
CTI__KEY_PLAY,
CTI__KEY_PAUSE,
CTI__KEY_RED,
CTI__KEY_GREEN,
CTI__KEY_YELLOW,
CTI__KEY_BLUE,
CTI__KEY_RECORD,
CTI__KEY_STOP,
CTI__KEY_MENU,
CTI__KEY_EXIT,
CTI__KEY_UP,
CTI__KEY_DOWN,
CTI__KEY_LEFT,
CTI__KEY_RIGHT,
CTI__KEY_NUMERIC_POUND,
CTI__KEY_NUMERIC_STAR,
CTI__KEY_FASTFORWARD,
CTI__KEY_REWIND,
CTI__KEY_BACK,
CTI__KEY_FORWARD,
CTI__KEY_HOME,
CTI__KEY_POWER,
CTI__KEY_0,
CTI__KEY_1,
CTI__KEY_2,
CTI__KEY_3,
CTI__KEY_4,
CTI__KEY_5,
CTI__KEY_6,
CTI__KEY_7,
CTI__KEY_8,
CTI__KEY_9,
CTI__KEY_A,
CTI__KEY_B,
CTI__KEY_C,
CTI__KEY_D,
CTI__KEY_E,
CTI__KEY_F,
CTI__KEY_G,
CTI__KEY_H,
CTI__KEY_I,
CTI__KEY_J,
CTI__KEY_K,
CTI__KEY_L,
CTI__KEY_M,
CTI__KEY_N,
CTI__KEY_O,
CTI__KEY_P,
CTI__KEY_Q,
CTI__KEY_R,
CTI__KEY_S,
CTI__KEY_T,
CTI__KEY_U,
CTI__KEY_V,
CTI__KEY_W,
CTI__KEY_X,
CTI__KEY_Y,
CTI__KEY_Z,
CTI__KEY_PLUS,
CTI__KEY_MINUS,
CTI__KEY_EQUALS,
CTI__KEY_CARET,
CTI__KEY_SPACE,
CTI__KEY_CAMERA,
};

typedef struct {
  int keycode;
} Keycode_message;

extern Keycode_message* Keycode_message_new(int keycode);
extern void Keycode_message_cleanup(Keycode_message **);

int Keycode_from_string(const char *string);
const char * Keycode_to_string(int keycode);
int Keycode_from_linux_event(uint16_t code);

#endif
