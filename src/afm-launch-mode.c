/*
 Copyright 2015 IoT.bzh

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

#include <string.h>

#include "afm-launch-mode.h"

static const char s_mode_local[] = "local";
static const char s_mode_remote[] = "remote";

enum afm_launch_mode launch_mode_of_string(const char *s)
{
	if (s) {
		if (!strcmp(s, s_mode_local))
			return mode_local;
		if (!strcmp(s, s_mode_remote))
			return mode_remote;
	}
	return invalid_launch_mode;
}

const char *name_of_launch_mode(enum afm_launch_mode m)
{
	switch (m) {
	case mode_local:  return s_mode_local;
	case mode_remote: return s_mode_remote;
	default:          return "(INVALID LAUNCH MODE)";
	}
}
