/* initctl: A replacement for systemd-initctl
 *
 * Reads commands from a fifo and calls systemctl.
 *
 * Copyright 2016-2026 Mike Gilbert
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <systemd/sd-bus.h>
#include <systemd/sd-daemon.h>

#include "initreq.h"

static bool init_halt = false;

static void set_environment(char *data, size_t size) {
	data[size - 1] = '\0';
	for (char *end = data + size; data && data < end && *data; data = strchr(data, '\0') + 1) {
		/* We only care about the INIT_HALT variable */
		if (!strcmp(data, "INIT_HALT=HALT"))
			init_halt = true;
		else if (!strcmp(data, "INIT_HALT") || !strncmp(data, "INIT_HALT=", 10))
			init_halt = false;
	}
}

static int bus_call(const char *method, const char *types, ...) {
	static sd_bus *bus = NULL;
	int r;

	for (int tries = 2; tries > 0; --tries) {
		if (bus == NULL) {
			r = sd_bus_new(&bus);
			if (r < 0)
				return r;

			r = sd_bus_set_address(bus, "unix:path=/run/systemd/private");
			if (r < 0)
				return r;

			r = sd_bus_start(bus);
			if (r < 0)
				return r;
		}

		va_list ap;
		va_start(ap, types);

		r = sd_bus_call_methodv(bus,
			"org.freedesktop.systemd1",
			"/org/freedesktop/systemd1",
			"org.freedesktop.systemd1.Manager",
			method, NULL, NULL, types, ap);

		va_end(ap);

		if (r == -ECONNRESET || r == -ENOTCONN) {
			bus = sd_bus_unref(bus);
			if (r == -ENOTCONN)
				continue;
		}

		break;
	}

	return r;
}

static void reload(void) {
	int r = bus_call("Reload", "");
	if (r < 0)
		fprintf(stderr, SD_ERR "Reload failed: %s\n", strerror(-r));
}

static void reexec(void) {
	int r = bus_call("Reexecute", "");
	if (r < 0 && r != -ECONNRESET)
		fprintf(stderr, SD_ERR "Reexecute failed: %s\n", strerror(-r));
}

static void start_unit(const char *name, const char *mode) {
	int r = bus_call("StartUnit", "ss", name, mode);
	if (r < 0)
		fprintf(stderr, SD_ERR "StartUnit(%s) failed: %s\n", name, strerror(-r));
}

static void change_runlevel(int runlevel) {
	switch (runlevel) {
		case '0':
			start_unit(init_halt ? "halt.target" : "poweroff.target",
					"replace-irreversibly");
			break;
		case '1':
		case 'S':
		case 's':
			start_unit("rescue.target", "isolate");
			break;
		case '2':
		case '3':
		case '4':
			start_unit("multi-user.target", "isolate");
			break;
		case '5':
			start_unit("graphical.target", "isolate");
			break;
		case '6':
			start_unit("reboot.target", "replace-irreversibly");
			break;
		case 'Q':
		case 'q':
			reload();
			break;
		case 'U':
		case 'u':
			reexec();
			break;
		default:
			fprintf(stderr, SD_WARNING "Got request for unknown runlevel '%c', ignoring.\n", runlevel);
	}
}

int main(void) {
	int r = sd_listen_fds(1);
	if (r != 1) {
		if (r < 0)
			fprintf(stderr, SD_ERR "Error calling sd_listen_fds: %s\n", strerror(-r));
		else
			fputs(SD_ERR "Wrong number of file descriptors.\n", stderr);
		return EX_NOINPUT;
	}

	int fd = SD_LISTEN_FDS_START;

	for (;;) {
		struct pollfd pfd = { .fd = fd, .events = POLLIN };
		int n = poll(&pfd, 1, 30000);

		if (n < 0) {
			perror(SD_ERR "Error waiting for input");
			return EX_IOERR;
		}

		if (n == 0 || !(pfd.revents & POLLIN))
			break;

		struct init_request req;
		ssize_t s = read(fd, &req, sizeof(req));

		if (s < 0) {
			perror(SD_ERR "Error reading from pipe");
			return EX_IOERR;
		}

		if (s == 0)
			break;

		if (s != sizeof(req) || req.magic != INIT_MAGIC) {
			fputs(SD_WARNING "Received bogus request.\n", stderr);
			continue;
		}

		switch (req.cmd) {
			case INIT_CMD_RUNLVL:
				change_runlevel(req.runlevel);
				break;
			case INIT_CMD_SETENV:
			case INIT_CMD_UNSETENV:
				set_environment(req.i.data, sizeof(req.i.data));
				break;
		}
	}

	return EX_OK;
}
