services_LTLIBRARIES = libflickr.la
libflickr_la_SOURCES = module.c mojito-service-flickr.c mojito-service-flickr.h
libflickr_la_CFLAGS = -I$(top_srcdir) $(REST_CFLAGS) $(GCONF_CFLAGS) $(SOUP_CFLAGS) -DG_LOG_DOMAIN=\"Flickr\"
libflickr_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(top_builddir)/mojito-keyfob/libmojito-keyfob.la $(top_builddir)/mojito-keystore/libmojito-keystore.la $(REST_LIBS) $(GCONF_LIBS) $(SOUP_LIBS)
libflickr_la_LDFLAGS = -module -avoid-version

servicesdata_DATA = flickr.keys
CLEANFILES = flickr.keys
EXTRA_DIST = flickr.keys.in
@INTLTOOL_MOJITO_KEYS@
