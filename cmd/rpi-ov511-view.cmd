new MjpegDemux mjd
new DO511 do511
new SDLstuff sdl

connect mjd O511_buffer do511
#connect mjd Wav_buffer ap

connect sdl Feedback_buffer mjd
connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 mjd:Keycode_msg

config sdl mode OVERLAY
#config sdl width 1440
#config sdl height 960

#connect dj RGB3_buffer vs
#connect vs RGB3_buffer sdl
#connect dj RGB3_buffer sdl

config do511 gspca_mode 1
connect do511 RGB3_buffer sdl

#config mjd input ./rcv
# Port 6667 is full speed, port 6668 is every-other.
config mjd input 192.168.2.80:6667
#config mjd input 127.0.0.1:6667
#config mjd input 192.168.2.109:6668
#config mjd input 127.0.0.1:5000:L

config mjd enable 1
