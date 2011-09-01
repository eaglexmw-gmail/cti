default: default1

MAIN=main.o

include ../platforms.make

HOSTARCH=$(shell uname -m)-$(shell uname -s)

ifeq ($(ARCH),$(HOSTARCH))
LDFLAGS+=-Wl,-rpath,$(shell pwd)/../../platform/$(ARCH)/lib
endif

CFLAGS += -g -Wall $(CMDLINE_CFLAGS)
# -std=c99 
CPPFLAGS += -I../../platform/$(ARCH)/include -I../jpeg-7
CPPFLAGS += -MMD -MP -MF $(OBJDIR)/$(subst .c,.dep,$<)
LDFLAGS += -L../../platform/$(ARCH)/lib -ljpeg -lpthread
ifeq ($(OS),Linux)
LDFLAGS += -lasound -ldl -lrt
endif

# "-static" is a problem for alsa, and other things...

OBJDIR ?= .

#default1: $(OBJDIR)/avcap$(EXEEXT) $(OBJDIR)/avidemux$(EXEEXT) $(OBJDIR)/cjbench$(EXEEXT) $(OBJDIR)/dvdgen$(EXEEXT) $(OBJDIR)/avtest$(EXEEXT) $(OBJDIR)/demux$(EXEEXT) $(OBJDIR)/alsacaptest$(EXEEXT) $(OBJDIR)/mjxtomp2$(EXEEXT) $(OBJDIR)/cti$(EXEEXT)

default1:  $(OBJDIR)/cti$(EXEEXT)

#	@echo wd=$(shell pwd)
#	@echo VPATH=$(VPATH)

# Another app.
OBJS= \
	$(OBJDIR)/CTI.o \
	$(OBJDIR)/locks.o \
	$(OBJDIR)/Mem.o \
	$(OBJDIR)/Index.o \
	$(OBJDIR)/Cfg.o \
	$(OBJDIR)/Log.o \
	$(OBJDIR)/Array.o \
	$(OBJDIR)/Range.o \
	$(OBJDIR)/File.o \
	$(OBJDIR)/jmemsrc.o \
	$(OBJDIR)/jmemdst.o \
	$(OBJDIR)/jpeghufftables.o \
	$(OBJDIR)/wrmem.o \
	$(OBJDIR)/CJpeg.o \
	$(OBJDIR)/DJpeg.o \
	$(OBJDIR)/DO511.o \
	$(OBJDIR)/ov511_decomp.o \
	$(OBJDIR)/Images.o \
	$(OBJDIR)/JpegFiler.o \
	$(OBJDIR)/JpegTran.o \
	$(OBJDIR)/MotionDetect.o \
	$(OBJDIR)/MjpegMux.o \
	$(OBJDIR)/MjpegDemux.o \
	$(OBJDIR)/SourceSink.o \
	$(OBJDIR)/String.o \
	$(OBJDIR)/ScriptV00.o \
	$(OBJDIR)/Wav.o \
	$(OBJDIR)/Audio.o \
	$(OBJDIR)/Numbers.o \
	$(OBJDIR)/Null.o \
	$(OBJDIR)/Control.o \
	$(OBJDIR)/Effects01.o \
	$(OBJDIR)/Collection.o \
	$(OBJDIR)/SocketServer.o \
	$(OBJDIR)/VSmoother.o \
	$(OBJDIR)/Y4MInput.o \
	$(OBJDIR)/Y4MOutput.o \
	$(OBJDIR)/XArray.o \
	$(OBJDIR)/JpegSource.o \
	$(OBJDIR)/HalfWidth.o \
	$(OBJDIR)/Mp2Enc.o \
	$(OBJDIR)/Mpeg2Enc.o \
	$(OBJDIR)/VFilter.o \
	$(OBJDIR)/AudioLimiter.o \
	$(OBJDIR)/Cryptor.o \
	$(OBJDIR)/MpegTSMux.o \
	$(OBJDIR)/MpegTSDemux.o \
	$(OBJDIR)/Tap.o \
	$(OBJDIR)/Keycodes.o \
	$(OBJDIR)/Pointer.o \
	$(OBJDIR)/ResourceMonitor.o \
	$(OBJDIR)/TV.o \
	$(OBJDIR)/ChannelMaps.o \
	$(OBJDIR)/Spawn.o \
	$(OBJDIR)/XPlaneControl.o \
	$(OBJDIR)/FaceTracker.o \
	$(OBJDIR)/Position.o \
	$(OBJDIR)/UI001.o \
	$(OBJDIR)/WavOutput.o \
	$(OBJDIR)/MjxRepair.o \
	$(OBJDIR)/cti_main.o \
	$(OBJDIR)/$(MAIN) \
	../../platform/$(ARCH)/jpeg-7/transupp.o

#	$(OBJDIR)/ScriptSession.o \


ifeq ($(OS),Linux)
OBJS+=\
	$(OBJDIR)/Uvc.o \
	$(OBJDIR)/V4L2Capture.o \
	$(OBJDIR)/ALSAio.o \
	$(OBJDIR)/ALSAMixer.o \
	$(OBJDIR)/FFmpegEncode.o \
	$(OBJDIR)/SonyPTZ.o \
	../../platform/$(ARCH)/jpeg-7/libjpeg.la
LDFLAGS+=-lvisca
endif

# SDL
ifeq ($(ARCH),armeb)
SDLLIBS=
else

ifeq ($(ARCH),i386-Darwin)
MAIN=SDLMain.o
OBJS+=$(OBJDIR)/sdl_main.o
endif


OBJS+=$(OBJDIR)/SDLstuff.o
CPPFLAGS+=$$(sdl-config --cflags) -I/usr/include/GL
LDFLAGS+=$$(sdl-config --libs) $$(pkg-config glu --libs)
endif

# Signals
ifneq ($(ARCH),armeb)
OBJS+=$(OBJDIR)/Signals.o
endif

# Lirc
ifneq ($(ARCH),armeb)
OBJS+= $(OBJDIR)/Lirc.o
LDFLAGS+=-llirc_client
endif


# Cairo
ifneq ($(ARCH),armeb)
OBJS+=	$(OBJDIR)/CairoContext.o
CPPFLAGS+=$$(pkg-config cairo --cflags)
LDFLAGS+=$$(pkg-config cairo --libs)
endif

# libdv
ifneq ($(ARCH),armeb)
OBJS+=	$(OBJDIR)/LibDV.o
CPPFLAGS+=$$(pkg-config libdv --cflags)
LDFLAGS+=$$(pkg-config libdv --libs)
endif

# Quicktime
ifneq ($(ARCH),armeb)
OBJS+=$(OBJDIR)/LibQuickTimeOutput.o
CPPFLAGS+=$$(pkg-config libquicktime --cflags)
LDFLAGS+=$$(pkg-config libquicktime --libs)
endif

# Ogg
ifneq ($(ARCH),armeb)
OBJS+=$(OBJDIR)/OggOutput.o
CPPFLAGS+=$$(pkg-config vorbisenc theoraenc --cflags)
LDFLAGS+=$$(pkg-config vorbisenc theoraenc --libs)
endif

# H264
ifneq ($(ARCH),armeb)
OBJS+=$(OBJDIR)/H264.o
CPPFLAGS+=$$(pkg-config x264 --cflags)
LDFLAGS+=$$(pkg-config x264 --libs)
endif

# AAC
ifneq ($(ARCH),armeb)
OBJS+=$(OBJDIR)/AAC.o
LDFLAGS+=-lfaac
endif


$(OBJDIR)/cti$(EXEEXT): \
	$(OBJS) \
	$(OBJDIR)/cti_app.o
	@echo LINK
	$(CC) $(filter %.o, $^) -o $@ $(LDFLAGS)
ifeq ($(ARCH),x86_64-Linux)
# Sigh, some libs bump their version numbers all the fucking time.  And I like to keep
# cti binaries around for later use, without always having to rebuild.  So, keep a
# cache of libraries which frequently change.
	@echo Copying required libaries that frequently change:
	@cp -Lvu $$(ldd $@ | grep -E '264|png' | sed -e 's,.*/usr,/usr,g' -e 's, .*$$,,') $(HOME)/lib/
# Or ../../platform/$(ARCH)/lib/
endif
#	$(STRIP) $@


$(OBJDIR)/mjplay$(EXEEXT): \
	$(OBJS) \
	$(OBJDIR)/mjplay.o
	@echo LINK
	$(CC) $(filter %.o, $^) -o $@ $(LDFLAGS)
#	$(STRIP) $@


$(OBJDIR)/ctest$(EXEEXT): \
	$(OBJDIR)/Collection.o \
	$(OBJDIR)/ctest.o
	$(CC) $(filter %.o, $^) -o $@



$(OBJDIR)/%.o: %.c Makefile
	@echo CC $@
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.m Makefile
	@echo CC $@
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -vf $(OBJDIR)/*.o  $(OBJDIR)/*.dep

DEPS=$(wildcard $(OBJDIR)/*.dep)
ifneq ($(DEPS),)
include $(DEPS)
endif
