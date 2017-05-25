AM_CFLAGS = \
	-I $(top_srcdir)/tests \
	-I $(top_srcdir)/include/drm \
	-I $(top_srcdir)/lima \
	-I $(top_srcdir)

LDADD = $(top_builddir)/libdrm.la \
	$(top_builddir)/lima/libdrm_lima.la

if HAVE_LIBPNG
AM_CFLAGS += $(LIBPNG_CFLAGS) -DLIMA_TEST_HAVE_LIBPNG
LDADD += $(LIBPNG_LIBS)
endif

if HAVE_INSTALL_TESTS
bin_PROGRAMS = \
	lima_test
else
noinst_PROGRAMS = \
	lima_test
endif

lima_test_SOURCES = \
	main.c
