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

#pragma once

extern int systemd_get_unit_path(char *path, size_t pathlen, int isuser, const char *unit, const char *uext);
extern int systemd_get_wants_path(char *path, size_t pathlen, int isuser, const char *wanter, const char *unit, const char *uext);
extern int systemd_get_wants_target(char *path, size_t pathlen, const char *unit, const char *uext);
extern int systemd_daemon_reload(int isuser);

