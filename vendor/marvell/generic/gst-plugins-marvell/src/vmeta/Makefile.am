# plugindir is set in configure

##############################################################################
# change libgstvmetadec.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgstvmetadec.la

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstvmetadec_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstvmetadec_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstvmetadec_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstvmetadec_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libgstvmetadec_la_SOURCES = gstvmetadec.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstvmetadec_la_CFLAGS = $(GST_CFLAGS)
libgstvmetadec_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(GSTCTRL_LIBS) -lcodecvmetadec -lvmetadove -lmiscgen
libgstvmetadec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstvmetadec_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstvmetadec.h
