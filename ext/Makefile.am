plugin_LTLIBRARIES = libgstrromx.la

# sources used to compile this plug-in
libgstrromx_la_SOURCES = \
	gstomx.c gstomx.h \
	gstomxpad.c gstomxpad.h \
	gstomxbase.c gstomxbase.h \
	gstomxbasedecoder.c gstomxbasedecoder.h\
	gstomxerror.c gstomxerror.h \
	gstomxbufferalloc.c gstomxbufferalloc.h \
	gstomxmpeg2dec.c gstomxmpeg2dec.h \
	gstomxh264dec.c gstomxh264dec.h \
	gstomxh264enc.c gstomxh264enc.h \
	gstomxjpegenc.c gstomxjpegenc.h \
	gstomxscaler.c gstomxscaler.h \
	gstomxbuftab.c gstomxbuftab.h \
	gstomxbufqueue.c gstomxbufqueue.h \
	gstomxdeiscaler.c gstomxdeiscaler.h \
	gstomxutils.c gstomxutils.h \
	gstomxbasesrc.c gstomxbasesrc.h \
	gstomxcamera.c gstomxcamera.h \
	gstomxaacenc.c gstomxaacenc.h \
	gstomxaacdec.c gstomxaacdec.h \
	gstomxrrparser.c gstomxrrparser.h \
	gstomxnoisefilter.c gstomxnoisefilter.h \
	gstperf.c gstperf.h

# compiler and linker flags used to compile this rromx, set in configure.ac
libgstrromx_la_CFLAGS = $(GST_CFLAGS) $(OMX_CFLAGS)
libgstrromx_la_LIBADD = $(GST_LIBS) $(OMX_LIBS)
libgstrromx_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstrromx_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = \
	gstomx.h \
	gstomxerror.h \
	gstomxpad.h \
	gstomxbase.h \
	gstomxbasedecoder.h \
	gstomxbufferalloc.h \
	gstomxmpeg2dec.h \
	gstomxh264dec.h \
	gstomxh264enc.h \
	gstomxjpegenc.h \
	gstomxscaler.h \
	gstomxbuftab.h \
	gstomxbufqueue.h \
	gstomxdeiscaler.h \
	gstomxutils.h \
	gstomxbasesrc.h \
	gstomxcamera.h \
	gstomxaacenc.h \
	gstomxaacdec.h \
	gstomxrrparser.h \
	gstomxnoisefilter.h
