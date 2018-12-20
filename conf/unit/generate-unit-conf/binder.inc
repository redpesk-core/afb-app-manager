dnl vim: set filetype=sysctl.conf.m4 syntax=sysctl.conf.m4:

IF_AGL_DEVEL
X-AFM-http-port={{:#metatarget.http-port}}
ELSE
IF_NOT_CONTENT(application/vnd.agl.service)
X-AFM--http-port={{:#metatarget.http-port}}
ENDIF
ENDIF

Type=notify
ExecStart=/usr/bin/afb-daemon \
	--name afbd-TARGET \
	--rootdir={{:#metadata.install-dir}} \
	--workdir=APP_DATA_DIR/{{id}} \
IF_AGL_DEVEL \
	--verbose \
	--verbose \
	--monitoring \
	--port={{:#metatarget.http-port}} \
	--token=HELLO \
	--roothttp=ON_CONTENT(application/vnd.agl.service, ., ON_PERM(:public:no-htdocs, ., htdocs)) \
ELSE \
	IF_CONTENT(application/vnd.agl.service) \
		--no-httpd \
	ELSE \
		--port={{:#metatarget.http-port}} \
		--random-token \
		--roothttp=ON_PERM(:public:no-htdocs, ., htdocs) \
	ENDIF \
ENDIF \
	ON_PERM(:public:applications:read, --alias=/icons:{{:#metadata.icons-dir}}) \
	{{#required-api}} \
		ON_VALUE(auto|ws,	--ws-client=unix:API_PATH_WS/{{name}}) \
		ON_VALUE(dbus,		--dbus-client={{name}}) \
		ON_VALUE(cloud,		--cloud-client={{name}}) \
		ON_VALUE(local,		--binding={{:#metadata.install-dir}}/{{name}}) \
		ON_VALUE(tcp,		--ws-client=tcp:{{name}}) \
	{{/required-api}} \
	{{#required-binding}} \
		ON_VALUE(local,		--binding={{:#metadata.install-dir}}/{{name}}) \
		ON_VALUE(extern,	--binding=API_PATH_LINK/{{name}}) \
	{{/required-binding}} \
	{{#provided-api}} \
		ON_VALUE(auto|ws,	--ws-server=sd:{{name}}) \
		ON_VALUE(dbus,		--dbus-server={{name}}) \
		ON_VALUE(tcp,		--ws-server=tcp:{{name}}) \
	{{/provided-api}} \
	ON_CONTENT(text/html,			--exec /usr/bin/web-runtime http://localhost:@p/{{content.src}}?token=@t) \
	ON_CONTENT(application/vnd.agl.native,	--exec {{:#metadata.install-dir}}/{{content.src}} @p @t)

