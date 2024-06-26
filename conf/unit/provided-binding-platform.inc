dnl vim: set filetype=sysctl.conf.m4 syntax=sysctl.conf.m4:
;-------------------------------------------------------------------------------
;----    T H E   S E R V I C E   O F   T H E   B I N D I N G       (USER)   ----
;-------------------------------------------------------------------------------
%begin systemd-unit

# auto generated by redpesk framework for {{:id}} version {{:version}} target {{:#target}} of {{:idaver}}

%systemd-unit system
%systemd-unit service UNIT_NAME_BINDING_BASE({{name}})@
%systemd-unit wanted-by afm-user-session@.target

[Unit]
Description=Provides binding {{name}} for user %i

BindsTo=afm-user-setup@%i.service
After=afm-user-setup@%i.service
Requires=UNIT_NAME_BINDING_BASE({{name}}).service
After=UNIT_NAME_BINDING_BASE({{name}}).service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/ln -sf FULL_PLATFORM_API_PATH_LINK/{{name}} FLOOR_USER_API_PATH_LINK/{{name}}

%end systemd-unit

;-------------------------------------------------------------------------------
;----    T H E   S E R V I C E   O F   T H E   B I N D I N G   (PLATFORM)   ----
;-------------------------------------------------------------------------------
%begin systemd-unit

# auto generated by redpesk framework for {{:id}} version {{:version}} target {{:#target}} of {{:idaver}}

%systemd-unit system
%systemd-unit service UNIT_NAME_BINDING_BASE({{name}})
%systemd-unit wanted-by multi-user.target

[Unit]
Description=Provides binding {{name}} for platform

Requires=afm-system-setup.service
After=afm-system-setup.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/ln -sf FULL_APP_INSTALL_DIR/{{value}} FLOOR_PLATFORM_API_PATH_LINK/{{name}}

%end systemd-unit
