new TV tv
new Lirc lirc
new V4L2Capture vc
new SDLstuff sdl
new ALSACapture ac
new ALSAPlayback ap
new ALSAMixer am
new CairoContext cc

config cc width 100
config cc height 100
config cc timeout 5
config cc command set_source_rgb 0.3 0.9 0.2
config cc command identity_matrix
config cc command move_to 10.0 60.0
config cc command set_font_size 50.0
config cc command show_text

connect vc RGB3_buffer cc
connect cc RGB3_buffer sdl
config sdl mode GL
# Zoom SDL viewport, this actually works!
config sdl width 1280
config sdl height 960

connect ac Wav_buffer ap

config ac device hw:2
config ac rate 48000
config ac channels 2
config ac format signed.16-bit.little.endian
config ac enable 1

config ap device hw:0
config ap rate 48000
config ap channels 2
config ap format signed.16-bit.little.endian
config ap enable 1

config am card M66

config vc device /dev/video0
config vc format BGR3
config vc size 640x480
config vc input Television
config vc mute 0
config vc Contrast 63
config vc enable 1

connect lirc Keycode_message tv
connect tv:VC_Config_msg vc:Config_msg
connect tv:Cairo_Config_msg cc:Config_msg
connect tv:Mixer_Config_msg am:Config_msg
