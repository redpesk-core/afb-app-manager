OPT = 
TOK = REDPESK_RPMPLUG_TRANSID

.PHONY: DEFAULT
DEFAULT: install

########################################
# cleaning
########################################
.PHONY: clean
clean:
	rm -rf plugins testdir example2-* .rpm.lock

########################################
# Build RPMs
########################################
RPM_CONTENT = README.md

RPMSUFFIX = -1.fc34.noarch.rpm

.PHONY: RPMS RPM1 RPM2
RPMS: RPM1 RPM2
RPM1: example2-1${RPMSUFFIX}
RPM2: example2-2${RPMSUFFIX}

example2-1${RPMSUFFIX}: example2-1.spec $(RPM_CONTENT)
	rpmbuild -bb --build-in-place $<
	mv ~/rpmbuild/RPMS/noarch/$@ .

example2-2${RPMSUFFIX}: example2-2.spec $(RPM_CONTENT)
	rpmbuild -bb --build-in-place $<
	mv ~/rpmbuild/RPMS/noarch/$@ .

example2-1.spec: example2.spec
	sed 's/Version:.*/Version: 1/' $< > $@

example2-2.spec: example2.spec
	sed 's/Version:.*/Version: 2/;s/f[123]/f&/g' $< > $@

########################################
# Build PLUGINs
########################################
.PHONY: PLUGIN
PLUGIN: plugins/draft_plugin.so

plugins:
	mkdir plugins

plugins/draft_plugin.so: draft-plugin.c plugins
	$(CC) -o $@ $< -lrpm -I../../external-includes -shared -fPIC -g

########################################
# Testing
########################################
.PHONY: install erase upgrade info reinstall

ROOTB = --root ${PWD}/testdir
ROOT = ${ROOTB}

testdir:
	mkdir testdir

install: testdir PLUGIN RPM1
	${TOK}=$$(date +%s.%N) \
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_draft_plugin %{__plugindir}/draft_plugin.so" \
		${ROOT} \
		--install ${OPT} example2-1-1.fc34.noarch.rpm

upgrade: testdir PLUGIN RPM2
	${TOK}=$$(date +%s.%N) \
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_draft_plugin %{__plugindir}/draft_plugin.so" \
		${ROOT} \
		--upgrade ${OPT} example2-2-1.fc34.noarch.rpm

downgrade: testdir PLUGIN RPM2
	${TOK}=$$(date +%s.%N) \
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_draft_plugin %{__plugindir}/draft_plugin.so" \
		${ROOT} \
		--upgrade --oldpackage ${OPT} example2-1-1.fc34.noarch.rpm

erase: testdir PLUGIN
	${TOK}=$$(date +%s.%N) \
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_draft_plugin %{__plugindir}/draft_plugin.so" \
		${ROOTB} \
		--erase ${OPT} example2

info: testdir PLUGIN
	${TOK}=$$(date +%s.%N) \
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_draft_plugin %{__plugindir}/draft_plugin.so" \
		${ROOTB} \
		--query ${OPT} --info example2

# reinstall is known to be broken
reinstall: testdir PLUGIN RPM1
	${TOK}=$$(date +%s.%N) \
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_draft_plugin %{__plugindir}/draft_plugin.so" \
		${ROOT} \
		--reinstall ${OPT} example2-1-1.fc34.noarch.rpm

