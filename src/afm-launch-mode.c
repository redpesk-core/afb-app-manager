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
#include <assert.h>

#include "afm-launch-mode.h"

/*
 * There is actually 2 possible launch mode:
 *  - local
 *  - remote
 */
static const char s_mode_local[] = "local";
static const char s_mode_remote[] = "remote";

/*
 * Records the current default mode
 */
static enum afm_launch_mode default_mode = mode_local;

/*
 * Set the default launch mode to 'mode'
 */
int is_valid_launch_mode(enum afm_launch_mode mode)
{
	switch(mode) {
	case mode_local:
	case mode_remote:
		return 1;
	default:
		return 0;
	}
}

/*
 * Get the default launch mode
 *
 * Ensure a valid result
 */
enum afm_launch_mode get_default_launch_mode()
{
	return default_mode;
}

/*
 * Set the default launch mode to 'mode'
 *
 * Requires 'mode' to be valid
 */
void set_default_launch_mode(enum afm_launch_mode mode)
{
	assert(is_valid_launch_mode(mode));
	default_mode = mode;
}

/*
 * Get the launch mode corresponding to the 'name'
 *
 * Returns invalid_launch_mode if the 'name' is not valid.
 */
enum afm_launch_mode launch_mode_of_name(const char *name)
{
	if (name) {
		if (!strcmp(name, s_mode_local))
			return mode_local;
		if (!strcmp(name, s_mode_remote))
			return mode_remote;
	}
	return invalid_launch_mode;
}

/*
 * Get the name of the launch 'mode'
 *
 * Requires 'mode' to be valid
 */
const char *name_of_launch_mode(enum afm_launch_mode mode)
{
	assert(is_valid_launch_mode(mode));
	switch (mode) {
	case mode_local:  return s_mode_local;
	case mode_remote: return s_mode_remote;
	default:          return NULL;
	}
}
