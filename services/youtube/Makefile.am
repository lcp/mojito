services_LTLIBRARIES = libyoutube.la
libyoutube_la_SOURCES = module.c youtube.c youtube.h
libyoutube_la_CFLAGS = -I$(top_srcdir) $(REST_CFLAGS) $(GCONF_CFLAGS) -DG_LOG_DOMAIN=\"Youtube\"
libyoutube_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(top_builddir)/mojito-keystore/libmojito-keystore.la $(REST_LIBS) $(GCONF_LIBS)
libyoutube_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = youtube.keys

servicesdata_DATA = youtube.keys
CLEANFILES = youtube.keys
EXTRA_DIST = youtube.keys.in
@INTLTOOL_MOJITO_KEYS@
