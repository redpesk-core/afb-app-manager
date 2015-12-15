/*
 Copyright 2015 IoT.bzh

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


struct jreq;
struct jbus;

extern struct jbus *create_jbus(int session, const char *path);
extern void jbus_addref(struct jbus *jbus);
extern void jbus_unref(struct jbus *jbus);

extern int jbus_replyj(struct jreq *jreq, const char *reply);
extern int jbus_reply(struct jreq *jreq, struct json_object *reply);
extern int jbus_add_service(struct jbus *jbus, const char *method, void (*oncall)(struct jreq *, struct json_object *));
extern int jbus_start_serving(struct jbus *jbus);
extern int jbus_read_write_dispatch(struct jbus *jbus, int toms);

extern int jbus_callj(struct jbus *jbus, const char *method, const char *query, void (*onresp)(int, struct json_object *, void *), void *data);
extern int jbus_call(struct jbus *jbus, const char *method, struct json_object *query, void (*onresp)(int, struct json_object *response, void *), void *data);

