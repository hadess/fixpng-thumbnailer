
bin_PROGRAMS = gnome-fixpng-thumbnailer

gnome_fixpng_thumbnailer_SOURCES = gnome-fixpng-thumbnailer.c gnome-thumbnailer-skeleton.c gnome-thumbnailer-skeleton.h
gnome_fixpng_thumbnailer_CPPFLAGS =		\
	$(THUMBNAILER_CFLAGS)			\
	-DTHUMBNAILER_RETURNS_PIXBUF		\
	-DTHUMBNAILER_USAGE="\"Thumbnail iOS-optimised PNGs\""
gnome_fixpng_thumbnailer_LDADD = $(THUMBNAILER_LIBS)

thumbnailerdir = $(datadir)/thumbnailers/
thumbnailer_DATA = gnome-fixpng-thumbnailer.thumbnailer
gnome-fixpng-thumbnailer.thumbnailer: gnome-fixpng-thumbnailer.thumbnailer.in Makefile
	$(AM_V_GEN) $(SED) -e "s|\@bindir\@|$(bindir)|" $< > $@

mimedir = $(datadir)/mime/packages
mime_DATA = image-x-apple-ios-png.xml

install-data-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif

uninstall-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif

DISTCHECK_CONFIGURE_FLAGS = --disable-update-mimedb

EXTRA_DIST = gnome-fixpng-thumbnailer.thumbnailer.in AUTHORS COPYING README image-x-apple-ios-png.xml

CLEANFILES = gnome-fixpng-thumbnailer.thumbnailer
