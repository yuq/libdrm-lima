include Makefile.sources

AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-I$(top_srcdir) \
	$(PTHREADSTUBS_CFLAGS) \
	-I$(top_srcdir)/include/drm

libdrm_lima_ladir = $(libdir)
libdrm_lima_la_LTLIBRARIES = libdrm_lima.la
libdrm_lima_la_LDFLAGS = -version-number 1:0:0 -no-undefined
libdrm_lima_la_LIBADD = \
	../libdrm.la \
	@PTHREADSTUBS_LIBS@ \
	@CLOCK_LIB@

libdrm_lima_la_SOURCES = $(LIBDRM_LIMA_FILES)

libdrm_limaincludedir = ${includedir}/libdrm
libdrm_limainclude_HEADERS = $(LIBDRM_LIMA_H_FILES)

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libdrm_lima.pc

TESTS = lima-symbol-check
EXTRA_DIST = $(TESTS)
