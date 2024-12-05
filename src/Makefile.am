## Process this file with automake to produce Makefile.in

AM_CFLAGS  = -Wno-deprecated-declarations -O3
AM_LDFLAGS = -lm
INCLUDES = \
	@GTK_CFLAGS@

bin_PROGRAMS = vol3d

vol3d_SOURCES = \
	main.c \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h

vold3d_DEPS = logo.xpm About.xpm

vol3d_LDADD = @GTK_LIBS@

