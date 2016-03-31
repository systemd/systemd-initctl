systemdutil_PROGRAMS = systemd-initctl
systemd_initctl_SOURCES = src/initctl.c src/initreq.h
systemd_initctl_LDADD = $(LIBSYSTEMD_LIBS)

dist_systemdsystemunit_DATA = units/systemd-initctl.socket
nodist_systemdsystemunit_DATA = units/systemd-initctl.service
EXTRA_DIST = units/systemd-initctl.service.in

units/systemd-initctl.service: units/systemd-initctl.service.in
	mkdir -p units
	sed -e 's,[@]systemdutildir[@],$(systemdutildir),g' \
		$(srcdir)/units/systemd-initctl.service.in \
		> units/systemd-initctl.service
