dnl vim: set filetype=sysctl.conf.m4 syntax=sysctl.conf.m4:
--------------------------------------------------------------------------------
-- GLOBAL IMPORTANT DEFINITIONS
--------------------------------------------------------------------------------
define( `X_EMITS', 1)
define( `X_DIVERTS', `divert(eval(-!X_EMITS()))')
define( `X_PUSH_DIVERTS', `pushdef(`X_EMITS', eval(X_EMITS() && $1))X_DIVERTS()')

define( `X_IF', `pushdef(`X_COND', $1)X_PUSH_DIVERTS($1)')
define( `X_ELSE', `popdef(`X_EMITS')X_PUSH_DIVERTS(eval(!X_COND))')
define( `X_ELIF', `X_ELSE()X_IF($1)')
define( `X_ENDIF', `popdef(`X_EMITS')popdef(`X_COND')X_DIVERTS()')

define( `IF', `pushdef(`ELSE',`X_ELSE()')pushdef(`ELIF',`X_ELIF($1)')pushdef(`ENDIF',`X_ENDIF()popdef(`ELSE',`ELIF',`ENDIF')')X_IF($1)')
--------------------------------------------------------------------------------
-- MUSTACH SPECIFIC DEFINITIONS
--------------------------------------------------------------------------------
define( `MUSTACH_ON', `ifelse(`$2',,,`{{#$1}}`$2'{{/$1}}')ifelse(`$3',,,`{{^$1}}`$3'{{/$1}}')')
define( `MUSTACH_IF', `{{#$1}}pushdef(`ELSE',`{{/$1}}{{^$1}}')pushdef(`ENDIF',`{{/$1}}popdef(`ELSE',`ENDIF')')')
define( `MUSTACH_IF_NOT', `{{^$1}}pushdef(`ELSE',`{{/$1}}{{#$1}}')pushdef(`ENDIF',`{{/$1}}popdef(`ELSE',`ENDIF')')')
--------------------------------------------------------------------------------
-- CONDITIONAL HELPERS
--------------------------------------------------------------------------------
define( `PERM', `AFM_PERM_PREFIX`'permission:$1')
define( `REQPERM', `required-permission.PERM($1)')

define( `ON_PERM', `MUSTACH_ON(REQPERM($1),$2,$3)')
define( `ON_NOT_PERM', `MUSTACH_ON(REQPERM($1),$3,$2)')
define( `IF_PERM', `MUSTACH_IF(REQPERM($1))')
define( `IF_NOT_PERM', `MUSTACH_IF_NOT(REQPERM($1))')

define( `ON_CONTENT', `MUSTACH_ON(content.type=$1,$2,$3)')
define( `IF_CONTENT', `MUSTACH_IF(content.type=$1)')
define( `IF_NOT_CONTENT', `MUSTACH_IF_NOT(content.type=$1)')

define( `ON_VALUE', `MUSTACH_ON(value=$1,$2,$3)')
define( `IF_VALUE', `MUSTACH_IF(value=$1)')
define( `IF_NOT_VALUE', `MUSTACH_IF_NOT(value=$1)')

define( `ON_REDPAK', `MUSTACH_ON(#metadata.redpak,$1,$2)')
define( `ON_NOT_REDPAK', `MUSTACH_ON(#metadata.redpak,$2,$1)')
define( `IF_REDPAK', `MUSTACH_IF(#metadata.redpak)')
define( `IF_NOT_REDPAK', `MUSTACH_IF_NOT(#metadata.redpak)')
--------------------------------------------------------------------------------
-- AFM DEFINITION
--------------------------------------------------------------------------------
-- NOTE:
--  when home screen will use real ids
--  use TARGET={{:id}}--{{:#target}}
--  instead of TARGET={{idaver}}{{^#target=main}}@{{:#target}}{{/#target=main}}
--------------------------------------------------------------------------------
define( `AFID', `{{:#metatarget.afid}}')
define( `SMACKLABEL', `App:{{:id}}')
define( `SELINUXLABEL', `system_u:system_r:{{:id-underscore}}_t:s0')
define( `TARGET', `{{idaver}}{{^#target=main}}@{{:#target}}{{/#target=main}}')
define( `NEWTARGET', `{{:id}}--{{:#target}}')
define( `UNITSUFFIX', `ON_NOT_PERM(`:partner:scope-platform', `@%i')')
define( `UNIT_NAME_BASE', `afm-ON_PERM(`:public:hidden', `service', `appli')-NEWTARGET')
define( `UNIT_NAME_API_BASE', `afm-api-$1')
define( `UNIT_NAME_API_SERVICE', `UNIT_NAME_API_BASE($1)UNITSUFFIX.service')
define( `UNIT_NAME_API_SOCKET', `UNIT_NAME_API_BASE($1)UNITSUFFIX.socket')
define( `UNIT_NAME_BINDING_BASE', `afm-link-$1')
define( `UNIT_NAME_BINDING_SERVICE', `UNIT_NAME_BINDING_BASE($1)UNITSUFFIX.service')

define( `REDPAK_ID', `{{:#metadata.redpak-id}}')
define( `ROOT_DIR', `{{:#metadata.root-dir}}')

define( `DEFPATH', `define(`$1', `ON_NOT_REDPAK(ROOT_DIR/)$2') define(`FULL_$1', `ROOT_DIR/$2')  define(`FLOOR_$1', `$2')' )
define( `DEFSUBPATH', `define(`$1', `$2/$3') define(`FULL_$1', `FULL_$2/$3') define(`FLOOR_$1', `FLOOR_$2/$3')' )

define( `REDWRAP', `ON_REDPAK(`/usr/bin/redwrap --redpath ROOT_DIR -- ')')
define( `BINDER', `ON_NOT_REDPAK(`/usr')/bin/afb-binder')

DEFPATH( `USER_RUN_DIR', `AFM_USERS_RUNDIR/%i')
DEFPATH( `DEBUGGING_DIR', `AFM_PLATFORM_RUNDIR/debug')
DEFPATH( `PLATFORM_RUN_DIR', `AFM_PLATFORM_RUNDIR')
DEFPATH( `CONF_DIR', `AFM_CONFDIR')
DEFPATH( `ICONS_DIR', `{{:#metadata.icons-dir}}')
DEFPATH( `WIDGET_DIR', `{{:#metadata.install-dir}}')

DEFPATH( `RUN_DIR', `ON_PERM(`:partner:scope-platform', `PLATFORM_RUN_DIR', `USER_RUN_DIR')')
DEFPATH( `APP_DATA_DIR', `ON_PERM(`:partner:scope-platform', `AFM_SCOPE_PLATFORM_HOMEDIR', `/home/%i/app-data')')

DEFSUBPATH( `APP_WORK_DIR', `APP_DATA_DIR', `{{:id}}')

DEFSUBPATH( `API_PATH', `RUN_DIR', `apis')
DEFSUBPATH( `API_PATH_WS', `API_PATH', `ws')
DEFSUBPATH( `API_PATH_LINK', `API_PATH', `link')

DEFSUBPATH( `PLATFORM_API_PATH', `PLATFORM_RUN_DIR', `apis')
DEFSUBPATH( `PLATFORM_API_PATH_WS', `PLATFORM_API_PATH', `ws')
DEFSUBPATH( `PLATFORM_API_PATH_LINK', `PLATFORM_API_PATH', `link')

DEFSUBPATH( `USER_API_PATH', `USER_RUN_DIR', `apis')
DEFSUBPATH( `USER_API_PATH_WS', `USER_API_PATH', `ws')
DEFSUBPATH( `USER_API_PATH_LINK', `USER_API_PATH', `link')

define( `LOCALUSERAPP', `ON_PERM(`:partner:scope-platform', `localuser---AFID', `localuser--AFID')')

define( `HTTP_PORT', `{{:#metatarget.http-port}}')
--------------------------------------------------------------------------------
-- AGL_DEVEL SPECIFIC PARTS
--------------------------------------------------------------------------------
define( `ON_AGL_DEVEL', `ifdef(`AGL_DEVEL', $1, $2)')
define( `ON_NOT_AGL_DEVEL', `ifdef(`AGL_DEVEL', $2, $1)')
define( `IF_AGL_DEVEL', `IF(ON_AGL_DEVEL(1,0))')
define( `IF_NOT_AGL_DEVEL', `IF(ON_AGL_DEVEL(0,1))')
--------------------------------------------------------------------------------
