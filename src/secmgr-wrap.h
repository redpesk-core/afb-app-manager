/*
 Copyright (C) 2015-2020 IoT.bzh

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



extern int secmgr_init(const char *id);
extern void secmgr_cancel();
extern int secmgr_install();
extern int secmgr_uninstall();
extern int secmgr_permit(const char *permission);

extern int secmgr_path_conf(const char *pathname);
extern int secmgr_path_data(const char *pathname);
extern int secmgr_path_exec(const char *pathname);
extern int secmgr_path_http(const char *pathname);
extern int secmgr_path_icon(const char *pathname);
extern int secmgr_path_lib(const char *pathname);
extern int secmgr_path_public(const char *pathname);
extern int secmgr_path_id(const char *pathname);
