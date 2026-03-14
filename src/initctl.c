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
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/wait.h>
#include <unistd.h>

#include <systemd/sd-daemon.h>

#include "initreq.h"

extern char **environ;
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

static void change_runlevel(int runlevel) {
	char *verb;

	switch (runlevel) {
		case '0':
			verb = init_halt ? "halt" : "poweroff";
			break;
		case '1':
		case 'S':
		case 's':
			verb = "rescue";
			break;
		case '2':
		case '3':
		case '4':
		case '5':
			verb = "default";
			break;
		case '6':
			verb = "reboot";
			break;
		case 'Q':
		case 'q':
			verb = "daemon-reload";
			break;
		case 'U':
		case 'u':
			verb = "daemon-reexec";
			break;
		default:
			fprintf(stderr, SD_WARNING "Got request for unknown runlevel '%c', ignoring.\n", runlevel);
			return;
	}

	char *argv[] = { "systemctl", verb, NULL };
	int r = posix_spawnp(NULL, argv[0], NULL, NULL, argv, environ);
	if (r)
		fprintf(stderr, SD_ERR "Failed to run systemctl: %s\n", strerror(r));
	else if (wait(NULL) < 0)
		fprintf(stderr, SD_ERR "Failed to wait for systemctl: %s\n", strerror(errno));
}

int main(void) {
	if (sd_listen_fds(1) != 1) {
		fputs(SD_ERR "Exactly one file descriptor must be passed from systemd.\n", stderr);
		return EX_NOINPUT;
	}

	int fd = SD_LISTEN_FDS_START;

	for (;;) {
		struct pollfd pfd = { .fd = fd, .events = POLLIN };
		int n = poll(&pfd, 1, 30000);

		if (n < 0) {
			fprintf(stderr, SD_ERR "Error waiting for input: %s\n", strerror(errno));
			return EX_IOERR;
		}

		if (n == 0 || !(pfd.revents & POLLIN))
			break;

		struct init_request req;
		ssize_t s = read(fd, &req, sizeof(req));

		if (s < 0) {
			fprintf(stderr, SD_ERR "Error reading from pipe: %s\n", strerror(errno));
			return EX_IOERR;
		}

		if (s == 0)
			break;

		if (s != sizeof(req) || req.magic != INIT_MAGIC) {
			fputs(SD_WARNING "Received bogus request\n", stderr);
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
