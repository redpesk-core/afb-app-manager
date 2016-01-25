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

extern char workdir[PATH_MAX];
extern int workdirfd;
extern void remove_workdir();
extern int set_workdir(const char *name, int create);
extern int make_workdir(const char *root, const char *prefix, int reuse);
extern int move_workdir(const char *dest, int parents, int force);
