dnl vim: set filetype=sysctl.conf.m4 syntax=sysctl.conf.m4:

IF_AGL_DEVEL
X-AFM-http-port={{:#metadata.http-port}}
ELSE
IF_NOT_CONTENT(application/vnd.agl.service)
X-AFM--http-port={{:#metadata.http-port}}
ENDIF
ENDIF

ExecStart=/usr/bin/afb-daemon \
	--name afbd-TARGET \
	--rootdir={{:#metadata.install-dir}} \
	--workdir=APP_DATA_DIR/{{id}} \
IF_AGL_DEVEL \
	--verbose \
	--verbose \
	--monitoring \
	--port={{:#metadata.http-port}} \
	--token=HELLO \
	--roothttp=ON_CONTENT(application/vnd.agl.service, ., ON_PERM(:public:no-htdocs, ., htdocs)) \
ELSE \
	IF_CONTENT(application/vnd.agl.service) \
		--no-httpd \
	ELSE \
		--port={{:#metadata.http-port}} \
		--random-token \
		--roothttp=ON_PERM(:public:no-htdocs, ., htdocs) \
	ENDIF \
ENDIF \
	ON_PERM(:public:applications:read, --alias=/icons:{{:#metadata.icons-dir}}) \
	{{#required-api}} \
		ON_VALUE(auto|ws,	--ws-client=unix:%t/apis/ws/{{name}}) \
		ON_VALUE(dbus,		--dbus-client={{name}}) \
		ON_VALUE(link,		--binding=%t/apis/lib/{{name}}) \
		ON_VALUE(cloud,		--cloud-client={{name}}) \
		ON_VALUE(local,		--binding={{:#metadata.install-dir}}/{{name}}) \
	{{/required-api}} \
	{{#provided-api}} \
		ON_VALUE(auto|ws,	--ws-server=sd:{{name}}) \
		ON_VALUE(dbus,		--dbus-server={{name}}) \
	{{/provided-api}} \
	ON_CONTENT(text/html,			--exec /usr/bin/web-runtime http://localhost:@p/{{content.src}}?token=@t) \
	ON_CONTENT(application/vnd.agl.native,	--exec {{:#metadata.install-dir}}/{{content.src}} @p @t)

