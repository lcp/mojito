services_LTLIBRARIES = libdigg.la
libdigg_la_SOURCES = module.c digg.c digg.h
libdigg_la_CFLAGS = -I$(top_srcdir) $(REST_CFLAGS) $(GCONF_CFLAGS) -DG_LOG_DOMAIN=\"Digg\"
libdigg_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(top_builddir)/mojito-keystore/libmojito-keystore.la $(REST_LIBS) $(GCONF_LIBS)
libdigg_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = digg.keys digg.png

servicesdata_DATA = digg.keys
CLEANFILES = digg.keys
EXTRA_DIST = digg.keys.in
@INTLTOOL_MOJITO_KEYS@
