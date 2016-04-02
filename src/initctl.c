/* initctl: A replacement for systemd-initctl
 *
 * Reads commands from /dev/initctl fifo.
 * Signals systemd for runlevel changes.
 *
 * Copyright 2016 Mike Gilbert
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

#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <systemd/sd-daemon.h>

#include "initreq.h"

static void change_runlevel(int runlevel) {
	int sig;

	switch (runlevel) {
		case '0':
			sig = SIGRTMIN+4; /* poweroff.target */
			break;
		case '1':
		case 'S':
		case 's':
			sig = SIGRTMIN+1; /* rescue.target */
			break;
		case '2':
		case '3':
		case '4':
		case '5':
			sig = SIGRTMIN+0; /* default.target */
			break;
		case '6':
			sig = SIGRTMIN+5; /* reboot.target */
			break;
		case 'Q':
		case 'q':
			sig = SIGHUP; /* daemon-reload */
			break;
		case 'U':
		case 'u':
			sig = SIGTERM; /* daemon-reexec */
			break;
		default:
			fprintf(stderr, "Got request for unknown runlevel %c, ignoring.", runlevel);
			return;
	}

	if (kill(1, sig) < 0)
		fprintf(stderr, "Error sending signal %d: %m\n", sig);
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
			perror("Error waiting for input");
			exit(EXIT_FAILURE);
		}

		if (n == 0)
			break;

		s = read(fd, &request, sizeof(request));

		if (s < 0) {
			perror("Error reading from pipe");
			exit(EXIT_FAILURE);
		}

		if (s == 0)
			break;

		if (s != sizeof(request) || request.magic != INIT_MAGIC) {
			fputs("Received bogus request\n", stderr);
			continue;
		}

		if (request.cmd == INIT_CMD_RUNLVL)
			change_runlevel(request.runlevel);

		/* TODO: Add error messaging for other commands */
	}
}

int main(void) {
	if (sd_listen_fds(false) != 1) {
		fputs("Exactly one file descriptor must be passed from systemd.\n", stderr);
		return EXIT_FAILURE;
	}

	sd_notify(false, "READY=1");
	process_requests(SD_LISTEN_FDS_START);
	close(SD_LISTEN_FDS_START);

	return 0;
}
