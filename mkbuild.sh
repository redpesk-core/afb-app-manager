#/bin/sh

h="$(dirname $0)"
force=false
: ${bd:=build}
: ${PREFIX:=$HOME/.local}
eval set -- $(getopt -o b:fp: -l buildir:,force,prefix: -- "$@") || exit
while :; do
	case "$1" in
	-b|--buildir) bd="$2"; shift;;
	-f|--force) force=true;;
	-p|--prefix) PREFIX="$2"; shift;;
	--) shift; break;;
	esac
	shift
done

cd "$h"
h=$(pwd)
mkdir -p "$bd" || exit
cd "$bd" || exit

$force && { rm -r * 2>/dev/null || rm CMakeCache.txt 2>/dev/null; }
test -f CMakeCache.txt -a -f Makefile || \
cmake \
	-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX:=$PREFIX} \
	-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:=Debug} \
	-DDISTINCT_VERSIONS=${DISTINCT_VERSIONS:=OFF} \
	-DUSE_LIBZIP=${USE_LIBZIP:=ON} \
	-DUSE_SDK=${USE_SDK:=OFF} \
	-DALLOW_NO_SIGNATURE=${ALLOW_NO_SIGNATURE:=OFF} \
	-DINSTALL_SAMPLE_KEYS=${INSTALL_SAMPLE_KEYS:=OFF} \
	-DWITH_RPM_REDPESK_PLUGIN=${WITH_RPM_REDPESK_PLUGIN:=ON} \
	-DWITH_TOOLS=${WITH_TOOLS:=ON} \
	-DWITH_CONFIG_XML=${WITH_CONFIG_XML:=ON} \
	-DWITH_LEGACY_WGTPKG=${WITH_LEGACY_WGTPKG:=OFF} \
	-DWITH_OPENSSL=${WITH_OPENSSL:=OFF} \
	-DSIMULATE_SECMGR=${SIMULATE_SECMGR:=OFF} \
	${afm_name+-Dafm_name="${afm_name}"} \
	${afm_confdir+-Dafm_confdir="${afm_confdir}"} \
	${afm_datadir+-Dafm_datadir="${afm_datadir}"} \
	${afm_libexecdir+-Dafm_libexecdir="${afm_libexecdir}"} \
	${afm_appdir+-Dafm_appdir="${afm_appdir}"} \
	${afm_icondir+-Dafm_icondir="${afm_icondir}"} \
	${afm_prefix+-Dafm_prefix="${afm_prefix}"} \
	${afm_units_root+-Dafm_units_root="${afm_units_root}"} \
	${crypto_trusted_certs_dir+-Dcrypto_trusted_certs_dir="${crypto_trusted_certs_dir}"} \
	${crypto_sample_keys_dir+-Dcrypto_sample_keys_dir="${crypto_sample_keys_dir}"} \
	${crypto_sample_certs_dir+-Dcrypto_sample_certs_dir="${crypto_sample_certs_dir}"} \
	${afm_platform_rundir+-Dafm_platform_rundir="${afm_platform_rundir}"} \
	${afm_users_rundir+-Dafm_users_rundir="${afm_users_rundir}"} \
	${afm_scope_platform_dir+-Dafm_scope_platform_dir="${afm_scope_platform_dir}"} \
	${rpm_plugin_dir+-Drpm_plugin_dir="${rpm_plugin_dir}"} \
	${AFMPKG_SOCKET_ADDRESS+-DAFMPKG_SOCKET_ADDRESS="${AFMPKG_SOCKET_ADDRESS}"} \
	${SYSCONFDIR_DBUS_SYSTEM+-DSYSCONFDIR_DBUS_SYSTEM="${SYSCONFDIR_DBUS_SYSTEM}"} \
	${SYSCONFDIR_PAMD+-DSYSCONFDIR_PAMD="${SYSCONFDIR_PAMD}"} \
	${UNITDIR_SYSTEM+-DUNITDIR_SYSTEM="${UNITDIR_SYSTEM}"} \
	"$h"

make -j "$@"
