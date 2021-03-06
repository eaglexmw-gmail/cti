# CTI Makefile

ARCH=$(shell uname -m)
OS=$(shell uname -s)

HOSTARCH=$(ARCH)-$(OS)
$(warning HOSTARCH=$(HOSTARCH))

ifeq ($(OS),Linux)
# Scale build to available cores.
MAKEFLAGS=j$(shell grep '^processor' /proc/cpuinfo | wc -l)
endif

ifeq ($(HOSTARCH),x86_64-Darwin)
MAKEFLAGS=j$(shell sysctl -n hw.logicalcpu)
LOCAL_JPEG=1
endif

ifeq ($(HOSTARCH),x86_64-Linux)
LOCAL_JPEG=1
LDFLAGS+=-lm
endif

ifeq ($(HOSTARCH),armv6l-Linux)
TURBO_JPEG=1
RPI=1
endif

ifeq ($(HOSTARCH),armv7l-Linux)
TURBO_JPEG=1
RPI=1
endif

ifeq ($(LOCAL_JPEG),1)
JPEGINCLUDE=-I../jpeg-8
JPEGLIB=../jpeg-8/.libs/libjpeg.a
JPEGTRANSUPP=../jpeg-8/transupp.o
endif

ifeq ($(TURBO_JPEG),1)
JPEGINCLUDE=-I../libjpeg-turbo -I../libjpeg-turbo/build
JPEGLIB=../libjpeg-turbo/build/libjpeg.a
JPEGTRANSUPP=../libjpeg-turbo/build/CMakeFiles/jpegtran-static.dir/transupp.c.o
endif

default: test-requirements cti_main

test-requirements:
	@test -f $(JPEGLIB) || ( echo "Need compiled libjpeg at $(JPEGLIB)" ; false )
#	@pkg-config --exists libcrypto || ( echo "Need libcrypto for ssl" ; false )
#	@pkg-config --exists libssl || ( echo "Need libssl for serf" ; false )

CC=gcc
CFLAGS=-O2
SHARED_FLAGS=-shared -fPIC
NM=nm
STRIP=strip
# ARCH:=$(shell uname -m)-$(shell uname -s)

MAIN=main.o

PKGCONFIGDIR ?= /usr/lib/pkgconfig

# This enables files > 2GB on 32-bit Linux.
CFLAGS += -D_FILE_OFFSET_BITS=64

CFLAGS += -Wall

ifeq ($(DEBUG),1)
CFLAGS += -O0 -ggdb
else
CFLAGS += -O2
endif

#CFLAGS += -Os 
#CFLAGS += -Werror
#CFLAGS += -O0 -ggdb
#CFLAGS += -Os -ggdb
CFLAGS += $(CMDLINE_CFLAGS)

ifneq ($(ARCH),armeb)
ifneq ($(ARCH),lpd)
ifneq ($(ARCH),i386-Darwin)
ifneq ($(ARCH),pentium3-Linux)
CFLAGS += -Wno-unused-result
endif
endif
endif
endif
# -std=c99
CPPFLAGS += -I/usr/include
#CPPFLAGS += -DLOCKS_STACKDEBUG
#CPPFLAGS += -I../platform/$(ARCH)/include
CPPFLAGS += -MMD -MP -MF $(subst .c,.dep,$<)
CPPFLAGS += $(JPEGINCLUDE)
LDFLAGS += $(JPEGLIB)
LDFLAGS += -lpthread
ifeq ($(OS),Linux)
LDFLAGS += -ldl -lrt
# rdynamic allows loaded .so files to see CTI global symbols.
LDFLAGS += -rdynamic
endif

# "-static" is a problem for alsa, and other things, so
# don't use it.

OBJDIR ?= .

cti_main: cti$(EXEEXT)
	@echo Done.

#	@echo wd=$(shell pwd)
#	@echo VPATH=$(VPATH)

# Another app.
OBJS= \
	CTI.o \
	locks.o \
	Mem.o \
	Index.o \
	Cfg.o \
	Log.o \
	ArrayU8.o \
	Range.o \
	File.o \
	jpeghufftables.o \
	CJpeg.o \
	DJpeg.o \
	jpeg_misc.o \
	DO511.o \
	ov511_decomp.o \
	Images.o \
	JpegFiler.o \
	JpegTran.o \
	$(JPEGTRANSUPP) \
	PGMFiler.o \
	MotionDetect.o \
	MjpegMux.o \
	MjpegDemux.o \
	SourceSink.o \
	String.o \
	CSV.o \
	ScriptV00.o \
	Wav.o \
	Audio.o \
	Numbers.o \
	Null.o \
	Effects01.o \
	SocketRelay.o \
	SocketServer.o \
	VSmoother.o \
	Y4MInput.o \
	Y4MOutput.o \
	JpegSource.o \
	RGB3Source.o \
	HalfWidth.o \
	Mp2Enc.o \
	Mpeg2Enc.o \
	VFilter.o \
	AudioLimiter.o \
	Cryptor.o \
	MpegTSMux.o \
	MpegTSDemux.o \
	Tap.o \
	Keycodes.o \
	Pointer.o \
	ResourceMonitor.o \
	TV.o \
	DTV.o \
	ChannelMaps.o \
	Spawn.o \
	XPlaneControl.o \
	FaceTracker.o \
	Position.o \
	UI001.o \
	WavOutput.o \
	MjxRepair.o \
	GdkCapture.o \
	MjpegLocalBuffer.o \
	MjpegStreamBuffer.o \
	ImageLoader.o \
	Images.o \
	FPS.o \
	NScale.o \
	Y4MSource.o \
	Splitter.o \
	cti_main.o \
	AVIDemux.o \
	RawSource.o \
	ImageOutput.o \
	SubProc.o \
	ExecProc.o \
	dpf.o \
	XMLMessageServer.o \
	XMLMessageRelay.o \
	socket_common.o \
	XmlSubset.o \
	nodetree.o \
	Stats.o \
	AudioFilter.o \
	HTTPServer.o \
	VirtualStorage.o \
	ATSCTuner.o \
	Y4MOverlay.o \
	VMixer.o \
	crc.o \
	Array.o \
	M3U8.o \
	HTTPClient.o \
	TunerControl.o \
	cti_utils.o \
	BinaryFiler.o \
	Global.o \
	UDPTransmit.o \
	Compositor.o \
	Alternator.o \
	Collator.o \
	ColorSpaceConvert.o \
	DynamicVideoControl.o \
	$(MAIN) 

include StackDebug.mk
OBJS+=StackDebug.o

ifeq ($(OS),Linux)
OBJS+=\
	Uvc.o \
	LinuxEvent.o \
	FFmpegEncode.o
ifneq ($(ARCH),lpd)
OBJS+=\
	V4L2Capture.o \
	ALSACapRec.o \
	ALSAio.o \
	ALSAMixer.o
LDFLAGS+=-lasound
CPPFLAGS+=-DHAVE_PRCTL
endif

#OBJS+=SonyPTZ.o
#CPPFLAGS+=-DHAVE_VISCA
#LDFLAGS+=-lvisca

ifeq ($(ARCH),lpd)
OBJS+=V4L1Capture.o
LDFLAGS+=-lm
endif

ifeq ($(ARCH),armeb)
CPPFLAGS+=-DHAVE_V4L1
OBJS+=V4L1Capture.o
endif

endif


# SDL on OSX
ifeq ($(ARCH),i386-Darwin)
OBJS+=sdl_main.o
OBJS+=SDLMain.o
endif

# Signals
#ifneq ($(ARCH),armeb)
OBJS+=Signals.o
#endif

# Lirc
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/../liblirc_client.so 2> /dev/null ))
OBJS+= Lirc.o
CPPFLAGS+=-DHAVE_LIRC
LDFLAGS+=-llirc_client
endif

#SDL
ifneq ($(ARCH),armeb)
ifneq ($(ARCH),lpd)
ifeq (0,$(shell (sdl-config --cflags > /dev/null 2> /dev/null; echo $$?)))
OBJS+=SDLstuff.o
CPPFLAGS+=$$(sdl-config --cflags) -I/usr/include/GL
LDFLAGS+=$$(sdl-config --libs) $$(pkg-config glu --libs)
CPPFLAGS+=-DHAVE_SDL
endif
endif
endif

# Cairo
ifneq ($(ARCH),i386-Darwin)
ifneq ($(ARCH),armeb)
ifneq ($(ARCH),lpd)
ifeq (0,$(shell (pkg-config cairo --cflags > /dev/null 2> /dev/null; echo $$?)))
OBJS+=	CairoContext.o
CPPFLAGS+=$$(pkg-config cairo --cflags)
LDFLAGS+=$$(pkg-config cairo --libs)
# This is a new requirement to resolve undefined reference to `pixman_*' errors
LDFLAGS+=$$(pkg-config pixman-1 --libs)
CPPFLAGS+=-DHAVE_CAIRO
endif
endif
endif
endif

# libdv
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/libdv.pc  2> /dev/null ))
OBJS+=	LibDV.o
CPPFLAGS+=$$(pkg-config libdv --cflags)
LDFLAGS+=$$(pkg-config libdv --libs)
CPPFLAGS+=-DHAVE_LIBDV
endif

# libpng
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/libpng.pc  2> /dev/null ))
OBJS+=	LibPng.o
CPPFLAGS+=$$(pkg-config libpng --cflags)
LDFLAGS+=$$(pkg-config libpng --libs)
CPPFLAGS+=-DHAVE_LIBPNG
endif

# libmpeg2
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/libmpeg2.pc  2> /dev/null ))
OBJS+=	Mpeg2Dec.o
CPPFLAGS+=$$(pkg-config libmpeg2 --cflags)
LDFLAGS+=$$(pkg-config libmpeg2 --libs)
CPPFLAGS+=-DHAVE_LIBMPEG2
endif

# Quicktime
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/libquicktime.pc  2> /dev/null ))
OBJS+=LibQuickTimeOutput.o
CPPFLAGS+=$$(pkg-config libquicktime --cflags)
LDFLAGS+=$$(pkg-config libquicktime --libs) -lpixman-1
CPPFLAGS+=-DHAVE_LIBQUICKTIME
endif

# Ogg
ifneq ($(ARCH),pentium3-Linux)
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/vorbisenc.pc 2> /dev/null ))
OBJS+=OggOutput.o
CPPFLAGS+=$$(pkg-config vorbisenc theoraenc --cflags)
LDFLAGS+=$$(pkg-config vorbisenc theoraenc --libs)
CPPFLAGS+=-DHAVE_OGGOUTPUT
endif
endif

# H264
ifneq ($(ARCH),pentium3-Linux)
ifeq ($(shell pkg-config --exists x264 && echo Y),Y)
OBJS+=H264.o
CPPFLAGS+=$$(pkg-config x264 --cflags)
LDFLAGS+=$$(pkg-config x264 --libs)
CPPFLAGS+=-DHAVE_H264
endif
endif

# Apache Serf
ifeq ($(shell pkg-config --exists serf-1 && echo Y),Y)
OBJS+=PushQueue.o
OBJS+=serf_get.o
CPPFLAGS+=$$(pkg-config serf-1 --cflags) $$(pkg-config --cflags apr-1) $$(pkg-config --cflags apr-util-1)
LDFLAGS+=$$(pkg-config --libs serf-1) $$(pkg-config --libs apr-1) $$(pkg-config --libs apr-util-1)
CPPFLAGS+=-DHAVE_SERF
endif

# libcurl
ifneq (,$(shell /bin/ls /usr/include/curl/curl.h 2> /dev/null ))
OBJS+=Curl.o
LDFLAGS+=-lcurl
CPPFLAGS+=-DHAVE_CURL
endif

# AAC
ifneq (,$(shell /bin/ls /usr/include/faac.h /usr/local/include/faac.h 2> /dev/null ))
OBJS+=AAC.o
LDFLAGS+=$(shell /bin/ls /usr/lib/libfaac.a /usr/lib/*/libfaac.a /usr/local/lib/libfaac.a 2> /dev/null )
CPPFLAGS+=-DHAVE_AAC
endif

# CUDA
ifeq ($(ARCH),x86_64-Linux)
ifneq (,$(shell /bin/ls /opt/cuda/bin/nvcc  2> /dev/null ))
OBJS+=NVidiaCUDA.o
LDFLAGS +=
CPPFLAGS+=
endif
endif

# OMX H264 encoder
../rpi-openmax-demos/cti-rpi-encode-yuv.o: ../rpi-openmax-demos/rpi-encode-yuv.c
	$(MAKE) -C ../rpi-openmax-demos cti_modules

ifeq ($(RPI),1)
OBJS+=RPiH264Enc.o ../rpi-openmax-demos/cti-rpi-encode-yuv.o
LDFLAGS += -L/opt/vc/lib -lopenmaxil -lbcm_host -lvcos -lpthread -lm
CPPFLAGS+=-DHAVE_RPIH264ENC
endif

# X11 globals
ifeq ($(shell pkg-config --exists x11 && echo Y),Y)
CPPFLAGS+=-DHAVE_X11
LDFLAGS += -lX11
endif


cti$(EXEEXT): \
	$(OBJS) \
	cti_app.o
	@echo LINK
	@echo $(CC) $(filter %.o, $^) -o $@ $(LDFLAGS)
	@$(CC) $(filter %.o, $^) -o $@ $(LDFLAGS)
	@echo Generating map
	$(NM) $@ | sort > $@.map
#	@cp --remove-destination -v $@ /platform/$(ARCH)/bin/
#	@echo STRIP
#	@$(STRIP) $@


SHARED_OBJS=$(subst .o,.so,$(OBJS))

ctis$(EXEEXT) : \
	$(SHARED_OBJS)
	@echo All shared objects are built.


mjplay$(EXEEXT): \
	$(OBJS) \
	mjplay.o
	@echo LINK
	$(CC) $(filter %.o, $^) -o $@ $(LDFLAGS)
#	$(STRIP) $@


%.o: %.c Makefile
	@echo CC $<
#	@echo $(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%.so: %.c Makefile
	@echo 'CC (dll)' $<
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(SHARED_FLAGS) -DCTI_SHARED $< -o $@

%.o: %.m Makefile
	@echo CC $<
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -vf *.o  *.dep cti

DEPS=$(wildcard *.dep)
ifneq ($(DEPS),)
include $(DEPS)
endif
