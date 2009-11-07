services_LTLIBRARIES = libfacebook.la
libfacebook_la_SOURCES = module.c facebook.c facebook.h
libfacebook_la_CFLAGS = -I$(top_srcdir) $(REST_CFLAGS) -DG_LOG_DOMAIN=\"Facebook\"
libfacebook_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(top_builddir)/mojito-keystore/libmojito-keystore.la $(top_builddir)/mojito-keyfob/libmojito-keyfob.la $(REST_LIBS)
libfacebook_la_LDFLAGS = -module -avoid-version

servicesdata_DATA = facebook.keys
CLEANFILES = facebook.keys
EXTRA_DIST = facebook.keys.in
@INTLTOOL_MOJITO_KEYS@
