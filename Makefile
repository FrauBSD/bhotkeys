############################################################ LICENSE
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
#
############################################################ IDENT(1)
#
# $Title: bhotkeys - grab-proof plugin host $
# $Copyright: 2026 Devin Teske. All rights reserved. $
# $FrauBSD: bhotkeys/Makefile 2026-10-07 12:10:30 -0700 Devin Teske $
#
############################################################ PROGRAMS

PROGRAM=	bhotkeys
PANEL=		${PROGRAM}-panel
PASSWD=		${PROGRAM}-passwd
START=		${PROGRAM}-start
GREETER=	${PROGRAM}-greeter

############################################################ PATHS

PREFIX?=	/usr/local
BINDIR?=	${PREFIX}/bin
LIBEXECDIR?=	${PREFIX}/libexec
SHAREDIR?=	${PREFIX}/share
ETCDIR?=	${PREFIX}/etc
MANDIR?=	${SHAREDIR}/man/man1

############################################################ PKG-CONFIG

PKG_CONFIG?=	pkg-config
PKGS=		x11 xext xtst xft
PKG_CFLAGS!=	${PKG_CONFIG} --cflags ${PKGS}
PKG_LIBS!=	${PKG_CONFIG} --libs ${PKGS}

############################################################ COMPILER

CC?=		cc
CFLAGS?=	-O2 -Wall -Wextra
CPPFLAGS+=	-Isrc -DPREFIX=\"${PREFIX}\" ${PKG_CFLAGS}

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -c -o ${.TARGET} ${.IMPSRC}

############################################################ LINKER

LDLIBS=		${PKG_LIBS}

############################################################ FILES

MANS=		${PROGRAM} ${PANEL} ${PASSWD} ${START} ${GREETER}
MAN=		man/${PROGRAM}.1 man/${PANEL}.1 man/${PASSWD}.1 \
		man/${START}.1 man/${GREETER}.1

# --print-keys spellers that share src/print_priv.h
SRCS_KEYS=	src/dwm_keys.c \
		src/fluxbox_keys.c \
		src/gnome_keys.c \
		src/i3_keys.c \
		src/kde_keys.c \
		src/lxqt_keys.c \
		src/mate_keys.c \
		src/openbox_keys.c \
		src/print_keys.c \
		src/wmaker_keys.c \
		src/xfce_keys.c \
		src/xmonad_keys.c

SRCS_PROGRAM= 	src/bind.c \
		src/exec.c \
		src/fvwm_keys.c \
		src/listen.c \
		src/main.c \
		src/plugins.c \
		src/session.c \
		src/wm.c \
		${SRCS_KEYS}

SRCS_PANEL=	src/bind.c \
		src/panel.c \
		src/panel_admin.c \
		src/panel_draw.c \
		src/panel_filter.c \
		src/panel_keys.c \
		src/panel_layout.c \
		src/panel_osk.c \
		src/panel_pw.c \
		src/panel_record.c \
		src/panel_touch.c \
		src/passwd.c \
		src/plugins.c \
		src/session.c \
		src/wm.c

SRCS_PASSWD=	src/${PASSWD}.c

OBJS_PROGRAM=	${SRCS_PROGRAM:.c=.o}
OBJS_PANEL=	${SRCS_PANEL:.c=.o}
OBJS_PASSWD=	${SRCS_PASSWD:.c=.o}

APPLY_SCRIPTS=	libexec/${PROGRAM}-bvwm-apply \
		libexec/${PROGRAM}-cinnamon-apply \
		libexec/${PROGRAM}-dwm-apply \
		libexec/${PROGRAM}-fluxbox-apply \
		libexec/${PROGRAM}-fvwm-apply \
		libexec/${PROGRAM}-gnome-apply \
		libexec/${PROGRAM}-i3-apply \
		libexec/${PROGRAM}-kde-apply \
		libexec/${PROGRAM}-lxde-apply \
		libexec/${PROGRAM}-lxqt-apply \
		libexec/${PROGRAM}-mate-apply \
		libexec/${PROGRAM}-openbox-apply \
		libexec/${PROGRAM}-windowmaker-apply \
		libexec/${PROGRAM}-xfce-apply \
		libexec/${PROGRAM}-xmonad-apply

PREFIX_SUBR=	libexec/${PROGRAM}-prefix.subr
AUTOSTART=	share/xdg/autostart/${PROGRAM}.desktop
I3CONF=		share/${PROGRAM}/i3.conf
GREETER_PA=	share/${PROGRAM}/greeter-system.pa
EXAMPLES=	xdm/Xsetup xdm/GiveConsole gdm/Init gdm/PostLogin \
		sddm/Xsetup sddm/pam
EXAMPLEDIR=	${SHAREDIR}/examples/${PROGRAM}
SUBR_SRC=	${PREFIX_SUBR} \
		libexec/${PROGRAM}-seq.subr \
		libexec/${PROGRAM}-plugin.subr \
		libexec/${PROGRAM}-openbox.subr

############################################################ TARGETS

.PHONY: all

all: ${PROGRAM} ${PANEL} ${PASSWD} bin/${START} bin/${GREETER} \
	${MAN} ${PREFIX_SUBR} ${AUTOSTART} ${I3CONF} \
	${EXAMPLES:C|^|examples/|}

${PREFIX_SUBR}: ${PREFIX_SUBR}.in Makefile
	sed -e 's|@PREFIX@|${PREFIX}|g' ${PREFIX_SUBR}.in > ${PREFIX_SUBR}
	chmod 644 ${PREFIX_SUBR}

bin/${START}: bin/${START}.in Makefile
	sed -e 's|@PREFIX@|${PREFIX}|g' bin/${START}.in > bin/${START}
	chmod 755 bin/${START}

bin/${GREETER}: bin/${GREETER}.in Makefile
	sed -e 's|@PREFIX@|${PREFIX}|g' bin/${GREETER}.in > bin/${GREETER}
	chmod 755 bin/${GREETER}

.for e in ${EXAMPLES}
examples/${e}: examples/${e}.in Makefile
	sed -e 's|@PREFIX@|${PREFIX}|g' examples/${e}.in > examples/${e}
.endfor

${AUTOSTART}: ${AUTOSTART}.in Makefile
	sed -e 's|@PREFIX@|${PREFIX}|g' ${AUTOSTART}.in > ${AUTOSTART}

${I3CONF}: ${I3CONF}.in Makefile
	sed -e 's|@PREFIX@|${PREFIX}|g' ${I3CONF}.in > ${I3CONF}

.for m in ${MANS}
man/${m}.1: man/${m}.1.in Makefile
	sed -e 's|@PREFIX@|${PREFIX}|g' man/${m}.1.in > man/${m}.1
.endfor

${OBJS_PROGRAM}: src/${PROGRAM}.h
${SRCS_KEYS:.c=.o}: src/print_priv.h

.for s in ${SRCS_PANEL}
.if empty(SRCS_PROGRAM:M${s}) && ${s:T} != "passwd.c"
${s:.c=.o}: src/${PROGRAM}.h src/panel_priv.h
.endif
.endfor

${PROGRAM}: ${OBJS_PROGRAM}
	${CC} ${CFLAGS} -o ${PROGRAM} ${OBJS_PROGRAM} ${LDLIBS}

${PANEL}: ${OBJS_PANEL}
	${CC} ${CFLAGS} -o ${PANEL} ${OBJS_PANEL} ${LDLIBS} \
		-lXrandr -lXi -lcrypt

${OBJS_PASSWD}: ${SRCS_PASSWD} src/bhotkeys.h
	${CC} ${CFLAGS} ${CPPFLAGS} -c -o ${.TARGET} ${SRCS_PASSWD}

${PASSWD}: ${OBJS_PASSWD}
	${CC} ${CFLAGS} -o ${PASSWD} ${OBJS_PASSWD} -lcrypt

.PHONY: install

install: all
	mkdir -p ${DESTDIR}${BINDIR} \
	    ${DESTDIR}${LIBEXECDIR}/${PROGRAM} \
	    ${DESTDIR}${SHAREDIR}/${PROGRAM}/plugins.d \
	    ${DESTDIR}${ETCDIR}/xdg/autostart \
	    ${DESTDIR}${MANDIR} \
	    ${DESTDIR}${EXAMPLEDIR}/xdm \
	    ${DESTDIR}${EXAMPLEDIR}/gdm \
	    ${DESTDIR}${EXAMPLEDIR}/sddm
	install -m 755 ${PROGRAM} ${PANEL} ${PASSWD} bin/${START} \
		bin/${GREETER} ${DESTDIR}${BINDIR}
	install -m 755 ${APPLY_SCRIPTS} \
		${DESTDIR}${LIBEXECDIR}/${PROGRAM}
	install -m 644 ${SUBR_SRC} \
		${DESTDIR}${LIBEXECDIR}/${PROGRAM}
	install -m 644 ${AUTOSTART} \
		${DESTDIR}${ETCDIR}/xdg/autostart/${PROGRAM}.desktop
	install -m 644 ${I3CONF} \
		${DESTDIR}${SHAREDIR}/${PROGRAM}/i3.conf
	install -m 644 ${GREETER_PA} \
		${DESTDIR}${SHAREDIR}/${PROGRAM}/greeter-system.pa
	install -m 644 examples/xdm/Xsetup examples/xdm/GiveConsole \
		${DESTDIR}${EXAMPLEDIR}/xdm
	install -m 644 examples/gdm/Init examples/gdm/PostLogin \
		${DESTDIR}${EXAMPLEDIR}/gdm
	install -m 644 examples/sddm/Xsetup examples/sddm/pam \
		${DESTDIR}${EXAMPLEDIR}/sddm
.for m in ${MANS}
	gzip -cn man/${m}.1 > ${DESTDIR}${MANDIR}/${m}.1.gz
.endfor
	chmod 444 ${DESTDIR}${MANDIR}/${PROGRAM}.1.gz \
	    ${DESTDIR}${MANDIR}/${PANEL}.1.gz \
	    ${DESTDIR}${MANDIR}/${PASSWD}.1.gz \
	    ${DESTDIR}${MANDIR}/${START}.1.gz \
	    ${DESTDIR}${MANDIR}/${GREETER}.1.gz

.PHONY: clean

clean:
	rm -f ${PROGRAM} ${PANEL} ${PASSWD} bin/${START} \
		bin/${GREETER} ${MAN} \
		${OBJS_PROGRAM} ${OBJS_PANEL} ${OBJS_PASSWD} \
		${PREFIX_SUBR} ${AUTOSTART} ${I3CONF} \
		${EXAMPLES:C|^|examples/|}

################################################################################
# END
################################################################################
