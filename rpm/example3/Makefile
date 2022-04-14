OPT = 

ENV = REDPESK_RPMPLUG_TRANSID=$$$$:$$(date +%s.%N) REDPESK_RPMPLUG_REDPAKID=@redpak$(shell echo $$PPID)


.PHONY: DEFAULT
DEFAULT: install

########################################
# cleaning
########################################
.PHONY: clean
clean:
	rm -rf plugins testdir example3-* .rpm.lock

########################################
# Build RPMs
########################################
RPM_CONTENT = README.md manifest.yml

RPMSUFFIX = -1.fc34.noarch.rpm

.PHONY: RPMS RPM1 RPM2
RPMS: RPM1 RPM2
RPM1: example3-1${RPMSUFFIX}
RPM2: example3-2${RPMSUFFIX}

example3-1${RPMSUFFIX}: example3-1.spec $(RPM_CONTENT)
	rpmbuild -bb --build-in-place $<
	mv ~/rpmbuild/RPMS/noarch/$@ .

example3-2${RPMSUFFIX}: example3-2.spec $(RPM_CONTENT)
	rpmbuild -bb --build-in-place $<
	mv ~/rpmbuild/RPMS/noarch/$@ .

example3-1.spec: example3.spec
	sed 's/Version:.*/Version: 1/' $< > $@

example3-2.spec: example3.spec
	sed 's/Version:.*/Version: 2/;s/f[123]/f&/g' $< > $@

########################################
# Build PLUGINs
########################################
.PHONY: PLUGIN
PLUGIN: plugins/redpesk.so

plugins:
	mkdir plugins

plugins/redpesk.so: redpesk.c plugins
	$(CC) -o $@ $< ../../src/detect-packtype.c -lrpm -I../../external-includes -I../../src/rpm-plugin -shared -fPIC -g

########################################
# Testing
########################################
.PHONY: install erase upgrade info reinstall

ROOTB = --root ${PWD}/testdir
ROOT = ${ROOTB}

testdir:
	mkdir testdir

install: testdir PLUGIN RPM1
	${ENV} \
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_redpesk %{__plugindir}/redpesk.so" \
		${ROOT} \
		--install ${OPT} example3-1-1.fc34.noarch.rpm

upgrade: testdir PLUGIN RPM2
	${ENV} \
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_redpesk %{__plugindir}/redpesk.so" \
		${ROOT} \
		--upgrade ${OPT} example3-2-1.fc34.noarch.rpm

downgrade: testdir PLUGIN RPM2
	${ENV} \
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_redpesk %{__plugindir}/redpesk.so" \
		${ROOT} \
		--upgrade --oldpackage ${OPT} example3-1-1.fc34.noarch.rpm

erase: testdir PLUGIN
	${ENV} \
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_redpesk %{__plugindir}/redpesk.so" \
		${ROOTB} \
		--erase ${OPT} example3

info: testdir PLUGIN
	${ENV} \
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_redpesk %{__plugindir}/redpesk.so" \
		${ROOTB} \
		--query ${OPT} --info example3

# reinstall is known to be broken
reinstall: testdir PLUGIN RPM1
	${ENV} \
	rpm -D "__plugindir ${PWD}/plugins" \
		-D "__transaction_redpesk %{__plugindir}/redpesk.so" \
		${ROOT} \
		--reinstall ${OPT} example3-1-1.fc34.noarch.rpm

