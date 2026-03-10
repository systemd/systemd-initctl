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
	/* We only care about the INIT_HALT variable */
	size_t maxlen;
	for (size_t i = 0; i < size && data[i]; i += 1 + strnlen(data + i, maxlen)) {
		maxlen = size - i;
		if (maxlen >= 15 && !strncmp(data + i, "INIT_HALT=HALT", 15))
			init_halt = true;
		else if (maxlen >= 10 && (!strncmp(data + i, "INIT_HALT", 10) ||
		                          !strncmp(data + i, "INIT_HALT=", 10)))
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

static void handle_request(struct init_request *request) {
	switch (request->cmd) {
		case INIT_CMD_RUNLVL:
			change_runlevel(request->runlevel);
			break;
		case INIT_CMD_SETENV:
		case INIT_CMD_UNSETENV:
			set_environment(request->i.data, sizeof(request->i.data));
			break;
	}
}

static void process_requests(int fd) {
	int n;
	ssize_t s;
	struct pollfd pfd;
	struct init_request request;

	pfd.fd = fd;
	pfd.events = POLLIN;

	for (;;) {
		n = poll(&pfd, 1, 30000);

		if (n < 0) {
			perror(SD_ERR "Error waiting for input");
			exit(EX_IOERR);
		}

		if (n == 0)
			break;

		s = read(fd, &request, sizeof(request));

		if (s < 0) {
			perror(SD_ERR "Error reading from pipe");
			exit(EX_IOERR);
		}

		if (s == 0)
			break;

		if (s != sizeof(request) || request.magic != INIT_MAGIC) {
			fputs(SD_WARNING "Received bogus request\n", stderr);
			continue;
		}

		handle_request(&request);
	}
}

int main(void) {
	if (sd_listen_fds(1) != 1) {
		fputs(SD_ERR "Exactly one file descriptor must be passed from systemd.\n", stderr);
		return EX_NOINPUT;
	}

	process_requests(SD_LISTEN_FDS_START);

	return EX_OK;
}
