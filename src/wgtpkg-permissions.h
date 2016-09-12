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

extern void reset_permissions();
extern void reset_requested_permissions();
extern void crop_permissions(unsigned level);
extern int grant_permission_list(const char *list);
extern int permission_exists(const char *name);
extern int request_permission(const char *name);
extern const char *first_usable_permission();
extern const char *next_usable_permission();

