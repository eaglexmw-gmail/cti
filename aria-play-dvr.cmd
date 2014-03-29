new MjpegDemux mjd
new DJpeg dj
new ALSAPlayback ap
new SDLstuff sdl
new JpegTran jt
new VSmoother vs
new VFilter vf

connect mjd O511_buffer do511
# Insert a JpegTran to clean up extraneous bytes.  Needed if feeding to
# libavcodec (ffmpeg, mplayer, etc.)
#connect mjd Jpeg_buffer jt
#connect jt Jpeg_buffer dj

# Or go direct and save the CPU cycles.
connect mjd Jpeg_buffer dj

connect mjd Wav_buffer ap

connect sdl Feedback_buffer mjd
connect sdl:Keycode_msg sdl:Keycode_msg
connect sdl:Keycode_msg_2 mjd:Keycode_msg

config sdl mode OVERLAY
#config sdl width 1440
#config sdl height 960

#connect dj RGB3_buffer vs
#connect vs RGB3_buffer sdl
#connect dj RGB3_buffer sdl
connect dj 422P_buffer vf
connect vf 422P_buffer sdl
connect do511 RGB3_buffer sdl

#config mjd input ./rcv
config mjd input 192.168.2.22:6667
#config mjd input 127.0.0.1:5000:L

config ap device plughw:2
config ap rate 48000
config ap channels 2
config ap format signed.16-bit.little.endian
config ap frames_per_io 128

config mjd enable 1
config ap enable 1