/*
 Copyright 2017 IoT.bzh

 author: José Bollo <jose.bollo@iot.bzh>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <assert.h>

#include <systemd/sd-bus.h>
#include <systemd/sd-bus-protocol.h>

#include "utils-systemd.h"

#if !defined(SYSTEMD_UNITS_ROOT)
# define SYSTEMD_UNITS_ROOT "/usr/local/lib/systemd"
#endif

static const char sdb_path[] = "/org/freedesktop/systemd1";
static const char sdb_destination[] = "org.freedesktop.systemd1";
static const char sdbi_manager[] = "org.freedesktop.systemd1.Manager";
static const char sdbm_reload[] = "Reload";

static struct sd_bus *sysbus;
static struct sd_bus *usrbus;

static int get_bus(int isuser, struct sd_bus **ret)
{
	int rc;
	struct sd_bus *bus;

	bus = isuser ? usrbus : sysbus;
	if (bus) {
		*ret = bus;
		rc = 0;
	} else if (isuser) {
		rc = sd_bus_default_user(ret);
		if (!rc)
			usrbus = *ret;
	} else {
		rc = sd_bus_default_system(ret);
		if (!rc)
			sysbus = *ret;
	}
	return rc;
}

int systemd_get_unit_path(char *path, size_t pathlen, int isuser, const char *unit, const char *uext)
{
	int rc = snprintf(path, pathlen, "%s/%s/%s.%s", 
			SYSTEMD_UNITS_ROOT,
			isuser ? "user" : "system",
			unit,
			uext);

	if (rc >= 0 && (size_t)rc >= pathlen) {
		errno = ENAMETOOLONG;
		rc = -1;
	}
	return rc;
}

int systemd_get_wants_path(char *path, size_t pathlen, int isuser, const char *wanter, const char *unit, const char *uext)
{
	int rc = snprintf(path, pathlen, "%s/%s/%s.wants/%s.%s", 
			SYSTEMD_UNITS_ROOT,
			isuser ? "user" : "system",
			wanter,
			unit,
			uext);

	if (rc >= 0 && (size_t)rc >= pathlen) {
		errno = ENAMETOOLONG;
		rc = -1;
	}
	return rc;
}

int systemd_get_wants_target(char *path, size_t pathlen, const char *unit, const char *uext)
{
	int rc = snprintf(path, pathlen, "../%s.%s", unit, uext);

	if (rc >= 0 && (size_t)rc >= pathlen) {
		errno = ENAMETOOLONG;
		rc = -1;
	}
	return rc;
}

int systemd_daemon_reload(int isuser)
{
	int rc;
	struct sd_bus *bus;

	rc = get_bus(isuser, &bus);
	if (!rc) {
		rc = sd_bus_call_method(bus, sdb_destination, sdb_path, sdbi_manager, sdbm_reload, NULL, NULL, NULL);
	}
	return rc;
}

