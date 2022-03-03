OPT = 

.PHONY: DEFAULT
DEFAULT: install

########################################
# cleaning
########################################
.PHONY: clean
clean:
	rm -rf plugins testdir example1-*

########################################
# Build RPMs
########################################
RPM_CONTENT = README.md

RPMSUFFIX = -1.fc34.noarch.rpm

.PHONY: RPMS RPM1 RPM2
RPMS: RPM1 RPM2
RPM1: example1-1${RPMSUFFIX}
RPM2: example1-2${RPMSUFFIX}

example1-1${RPMSUFFIX}: example1-1.spec $(RPM_CONTENT)
	rpmbuild -bb --build-in-place $<
	mv ~/rpmbuild/RPMS/noarch/$@ .

example1-2${RPMSUFFIX}: example1-2.spec $(RPM_CONTENT)
	rpmbuild -bb --build-in-place $<
	mv ~/rpmbuild/RPMS/noarch/$@ .

example1-1.spec: example1.spec
	sed 's/Version:.*/Version: 1/' $< > $@

example1-2.spec: example1.spec
	sed 's/Version:.*/Version: 2/' $< > $@

########################################
# Build PLUGINs
########################################
.PHONY: PLUGIN
PLUGIN: plugins/test_plugin.so

plugins:
	mkdir plugins

plugins/test_plugin.so: test-plugin.c plugins
	$(CC) -o $@ $< -lrpm -I../../external-includes -shared -fPIC

########################################
# Testing
########################################
.PHONY: install erase upgrade info reinstall

ROOTB = --root ${PWD}/testdir
ROOT = ${ROOTB}
#ROOTB = --dbpath ${PWD}/testdir
#ROOT = ${ROOTB} --prefix ${PWD}/testdir

testdir:
	mkdir testdir

install: testdir PLUGIN RPM1
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_test_plugin %{__plugindir}/test_plugin.so" \
		${ROOT} \
		--install ${OPT} example1-1-1.fc34.noarch.rpm

upgrade: testdir PLUGIN RPM2
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_test_plugin %{__plugindir}/test_plugin.so" \
		${ROOT} \
		--upgrade ${OPT} example1-2-1.fc34.noarch.rpm

erase: testdir PLUGIN
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_test_plugin %{__plugindir}/test_plugin.so" \
		${ROOTB} \
		--erase ${OPT} example1

info: testdir PLUGIN
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_test_plugin %{__plugindir}/test_plugin.so" \
		${ROOTB} \
		--query ${OPT} --info example1

# reinstall is known to be broken
reinstall: testdir PLUGIN RPM1
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_test_plugin %{__plugindir}/test_plugin.so" \
		${ROOT} \
		--reinstall ${OPT} example1-1-1.fc34.noarch.rpm

