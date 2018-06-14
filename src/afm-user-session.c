/*
 Copyright 2018 IoT.bzh

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

#include <stdio.h>
#include <unistd.h>
#include <systemd/sd-daemon.h>

extern char **environ;

int main()
{
	char buffer[100];
	char *args[] = { "/bin/systemctl", "--wait", "start", buffer, 0 };

	sprintf(buffer, "afm-user-session@%d.target", (int)getuid());
	sd_notify(0, "READY=1");
	execve(args[0], args, environ);
	return 1;
}
