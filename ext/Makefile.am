plugin_LTLIBRARIES = libgstrromx.la

# sources used to compile this plug-in
libgstrromx_la_SOURCES = \
	gstomx.c gstomx.h \
	gstomxpad.c gstomxpad.h \
	gstomxbase.c gstomxbase.h \
	gstomxerror.c gstomxerror.h \
	gstomxbufferalloc.c gstomxbufferalloc.h \
	gstomxmpeg2dec.c gstomxmpeg2dec.h \
	gstomxh264dec.c gstomxh264dec.h \
	gstomxh264enc.c gstomxh264enc.h \
	gstomxscaler.c gstomxscaler.h \
	gstomxbuftab.c gstomxbuftab.h \
	gstomxdeiscaler.c gstomxdeiscaler.h \
	gstomxutils.c gstomxutils.h

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
	gstomxbufferalloc.h \
	gstomxmpeg2dec.h \
	gstomxh264dec.h \
	gstomxh264enc.h \
	gstomxscaler.h \
	gstomxbuftab.h \
	gstomxdeiscaler.h \
	gstomxutils.h
