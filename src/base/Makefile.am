AUTOMAKE_OPTIONS=foreign
AM_CFLAGS=-DPREFIX="\"$(prefix)\""
INCLUDES=-I../lib
bin_PROGRAMS=plume
plume_SOURCES=main.c plm_conf.c plm_ctx.c plm_dispatcher.c plm_plugin_base.c
plume_LDADD=-L../lib -lplm_util -ldl
