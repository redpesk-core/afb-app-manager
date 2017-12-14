dnl vim: set filetype=sysctl.conf.m4 syntax=sysctl.conf.m4:
;---------------------------------------------------------------------------------
;----         M A I N    P A R T    O F   T H E   S E R V I C E               ----
;---------------------------------------------------------------------------------
%begin systemd-unit

# auto generated by wgtpkg-unit for {{:id}} version {{:version}} target {{:#target}} of {{:idaver}}
%nl

%systemd-unit system
%systemd-unit service UNIT_NAME_BASE

[Unit]
Description={{description}}
X-AFM-description={{description}}
X-AFM-name={{name.content}}
X-AFM-shortname={{name.short}}
X-AFM-id=TARGET
X-AFM-version={{:version}}
X-AFM-author={{author.content}}
X-AFM-author-email={{author.email}}
X-AFM-width={{width}}
X-AFM-height={{height}}
{{#icon}}
X-AFM-icon={{:#metadata.install-dir}}/{{:src}}
{{/icon}}
X-AFM--ID={{:id}}
X-AFM--target-name={{:#target}}
X-AFM--content={{content.src}}
X-AFM--type={{content.type}}
X-AFM--wgtdir={{:#metadata.install-dir}}
X-AFM--workdir=APP_DATA_DIR/{{:id}}
%nl

Requires=afm-user-session@%i.target
After=user@%i.service

# Adds check to smack
ConditionSecurity=smack
%nl

# Automatic bound to required api
{{#required-api}}
{{#value=auto|ws}}
BindsTo=UNIT_NAME_SOCKET_FOR({{name}})
After=UNIT_NAME_SOCKET_FOR({{name}})
{{/value=auto|ws}}
{{/required-api}}
{{#provided-api}}
{{#value=ws|auto}}
Requires=UNIT_NAME_SOCKET_FOR({{name}})
After=UNIT_NAME_SOCKET_FOR({{name}})
{{/value=ws|auto}}
{{/provided-api}}

%nl

[Service]
EnvironmentFile=-AFM_CONFIG_DIR/unit.env.d/*
SmackProcessLabel=User::App::{{:id}}
SuccessExitStatus=0 SIGKILL

User=%i
Slice=user-%i.slice

#CapabilityBoundingSet=
#AmbientCapabilities=

ON_PERM(:platform:no-oom,   OOMScoreAdjust=-500)
ON_PERM(:partner:real-time, IOSchedulingClass=realtime)
ON_PERM(:public:display,    SupplementaryGroups=display)
ON_PERM(:public:syscall:clock, , SystemCallFilter=~@clock)
%nl

WorkingDirectory=-APP_DATA_DIR/{{:id}}
ExecStartPre=/bin/mkdir -p APP_DATA_DIR/{{:id}}
Environment=AFM_APP_INSTALL_DIR={{:#metadata.install-dir}}
Environment=PATH=/usr/sbin:/usr/bin:/sbin:/bin:{{:#metadata.install-dir}}/bin
Environment=LD_LIBRARY_PATH={{:#metadata.install-dir}}/lib
Environment=XDG_DATA_HOME=APP_DATA_DIR/{{:id}}
Environment=XDG_CONFIG_HOME=APP_DATA_DIR/{{:id}}
Environment=XDG_CACHE_HOME=APP_DATA_DIR/{{:id}}
Environment=XDG_RUNTIME_DIR=/run/user/%i
Environment=DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/%i/bus

IF_AGL_DEVEL
; Needed to enable debug
Environment=AFM_ID=TARGET
EnvironmentFile=-/var/run/afm-debug/TARGET.env
ENDIF

SyslogIdentifier=afbd-TARGET
StandardInput=null
StandardOutput=journal
StandardError=journal

;---------------------------------------------------------------------------------
;----   text/html  application/vnd.agl.native  application/vnd.agl.service    ----
;---------------------------------------------------------------------------------
{{#content.type=text/html|application/vnd.agl.native|application/vnd.agl.service}}
include(binder.inc)
{{/content.type=text/html|application/vnd.agl.native|application/vnd.agl.service}}

;---------------------------------------------------------------------------------
;----                 application/x-executable                                ----
;---------------------------------------------------------------------------------
{{#content.type=application/x-executable}}
ExecStart={{:#metadata.install-dir}}/{{content.src}}
{{/content.type=application/x-executable}}

IF_PERM(:system:run-by-default)
;---------------------------------------------------------------------------------
; auto start
;---------------------------------------------------------------------------------
[Install]
WantedBy=afm-user-session@.target
%systemd-unit wanted-by afm-user-session@.target
ENDIF

%end systemd-unit

