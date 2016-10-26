/*
 Copyright 2015, 2016 IoT.bzh

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

enum afm_launch_mode {
	invalid_launch_mode = 0,
	mode_local   = 1,
	mode_remote  = 2
};

extern int is_valid_launch_mode(enum afm_launch_mode mode);

extern enum afm_launch_mode get_default_launch_mode();
extern void set_default_launch_mode(enum afm_launch_mode mode);

extern enum afm_launch_mode launch_mode_of_name(const char *name);
extern const char *name_of_launch_mode(enum afm_launch_mode mode);

