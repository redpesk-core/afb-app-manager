/*
 Copyright (C) 2015-2022 IoT.bzh Company

 Author: José Bollo <jose.bollo@iot.bzh>

 $RP_BEGIN_LICENSE$
 Commercial License Usage
  Licensees holding valid commercial IoT.bzh licenses may use this file in
  accordance with the commercial license agreement provided with the
  Software or, alternatively, in accordance with the terms contained in
  a written agreement between you and The IoT.bzh Company. For licensing terms
  and conditions see https://www.iot.bzh/terms-conditions. For further
  information use the contact form at https://www.iot.bzh/contact.

 GNU General Public License Usage
  Alternatively, this file may be used under the terms of the GNU General
  Public license version 3. This license is as published by the Free Software
  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
  of this file. Please review the following information to ensure the GNU
  General Public License requirements will be met
  https://www.gnu.org/licenses/gpl-3.0.html.
 $RP_END_LICENSE$
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "sighup-framework.h"
#include "utils-systemd.h"

static const char *afm_system_daemon = "afm-system-daemon";

void sighup_afm_main()
{
	struct dirent *de;
	char *end, buffer[300];
	DIR *d;
	int fd;
	ssize_t ssz;
	long pid;
	
	d = opendir("/proc");
	if (d != NULL) {
		while((de = readdir(d)) != NULL) {
			pid = strtol(de->d_name, &end, 10);
			if (*end == 0) {
				snprintf(buffer, sizeof buffer, "/proc/%s/cmdline", de->d_name);
				fd = open(buffer, O_RDONLY);
				if (fd >= 0) {
					do { ssz = read(fd, buffer, sizeof buffer); } while (ssz < 0 && errno == EINTR);
					close(fd);
					if(strstr(buffer, afm_system_daemon)) {
						kill((pid_t)pid, SIGHUP);
						break;
					}
				}
			}
		}
		closedir(d);
	}
}

void sighup_systemd()
{
	systemd_daemon_reload(0);
	systemd_unit_restart_name(0, "sockets.target", NULL);
}

void sighup_all()
{
	sighup_systemd();
	sighup_afm_main();
}
