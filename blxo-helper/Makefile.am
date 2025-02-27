
SUBDIRS =								\
	helpers

AM_CPPFLAGS = 								\
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-I$(top_srcdir)							\
	-DBINDIR=\"$(bindir)\"						\
	-DDATADIR=\"$(datadir)\"					\
	-DG_LOG_DOMAIN=\"blxo-helper\"					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

blxo_helperdir = $(HELPER_PATH_PREFIX)/xfce4/blxo-$(LIBBLXO2_VERSION_API)

blxo_helper_PROGRAMS = \
	blxo-helper-2

blxo_helper_built_sources =						\
	blxo-helper-enum-types.c						\
	blxo-helper-enum-types.h

blxo_helper_headers =							\
	blxo-helper.h

blxo_helper_2_SOURCES =						\
	$(blxo_helper_built_sources)					\
	$(blxo_helper_headers)						\
	blxo-helper-chooser-dialog.c					\
	blxo-helper-chooser-dialog.h					\
	blxo-helper-chooser.c						\
	blxo-helper-chooser.h						\
	blxo-helper-launcher-dialog.c					\
	blxo-helper-launcher-dialog.h					\
	blxo-helper-utils.c						\
	blxo-helper-utils.h						\
	blxo-helper.c							\
	main.c

blxo_helper_2_CFLAGS =							\
	$(GTK3_CFLAGS)							\
	$(GIO_UNIX_CFLAGS)							\
	$(LIBBLADEUTIL_CFLAGS)						\
	$(LIBBLADEUI2_CFLAGS)

blxo_helper_2_DEPENDENCIES =						\
	$(top_builddir)/blxo/libblxo-2.la

blxo_helper_2_LDADD =							\
	$(GTK3_LIBS)							\
	$(GIO_UNIX_LIBS)							\
	$(LIBBLADEUTIL_LIBS)						\
	$(LIBBLADEUI2_LIBS)						\
	$(top_builddir)/blxo/libblxo-2.la

bin_SCRIPTS =								\
	blxo-preferred-applications

blxo-preferred-applications: Makefile $(srcdir)/blxo-preferred-applications.in
	sed -e 's,@HELPERDIR\@,$(HELPER_PATH_PREFIX)/xfce4/blxo-$(LIBBLXO2_VERSION_API),g' \
	    -e 's,@LIBBLXO2_VERSION_API\@,$(LIBBLXO2_VERSION_API),g' 	\
	    < $(srcdir)/blxo-preferred-applications.in			\
	    > blxo-preferred-applications

desktopdir = $(datadir)/applications
desktop_in_files =							\
	blxo-preferred-applications.desktop.in
desktop_DATA =								\
	$(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

##
## Rules to auto-generate built sources
##
## This is a bit tricky with automake, and non-trivial to implement. The
## rules below seem to work fine and don't seem to break the build, but
## they are only enabled in maintainer mode, so arbitrary users don't get
## trapped in automake's oddities. Therefore we ship the autogenerated
## files as part of the dist tarball.
##
if MAINTAINER_MODE
DISTCLEANFILES =							\
	stamp-blxo-helper-enum-types.h					\
	$(blxo_helper_built_sources)

BUILT_SOURCES =								\
	$(blxo_helper_built_sources)

blxo-helper-enum-types.h: stamp-blxo-helper-enum-types.h
	@true
stamp-blxo-helper-enum-types.h: $(blxo_helper_headers) Makefile
	$(AM_V_GEN) ( cd $(srcdir) && glib-mkenums \
		--fhead "#ifndef __BLXO_HELPER_ENUM_TYPES_H__\n#define __BLXO_HELPER_ENUM_TYPES_H__\n#include <blxo/blxo.h>\nG_BEGIN_DECLS\n" \
		--fprod "/* enumerations from \"@filename@\" */\n" \
		--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define BLXO_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
		--ftail "G_END_DECLS\n\n#endif /* !__BLXO_HELPER_ENUM_TYPES_H__ */" \
		$(blxo_helper_headers) ) >> xgen-eeth \
	&& (cmp -s xgen-eeth blxo-helper-enum-types.h || cp xgen-eeth blxo-helper-enum-types.h ) \
	&& rm -f xgen-eeth \
	&& echo timestamp > $(@F)

blxo-helper-enum-types.c: $(blxo_helper_headers) Makefile
	$(AM_V_GEN) ( cd $(srcdir) && glib-mkenums \
		--fhead "#undef GTK_DISABLE_DEPRECATED\n#define GTK_ENABLE_BROKEN\n#include <blxo-helper/blxo-helper.h>\n\n#include \"blxo-helper-enum-types.h\"" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n\tstatic GType type = 0;\n\tif (type == 0) {\n\tstatic const G@Type@Value values[] = {"\
		--vprod "\t{ @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "\t{ 0, NULL, NULL }\n\t};\n\ttype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n\treturn type;\n}\n" \
		$(blxo_helper_headers) ) >> xgen-eetc \
	&& cp xgen-eetc blxo-helper-enum-types.c \
	&& rm -f xgen-eetc
endif

CLEANFILES =								\
	$(desktop_DATA)							\
	blxo-preferred-applications

EXTRA_DIST =								\
	$(desktop_in_files)						\
	blxo-preferred-applications.in

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
