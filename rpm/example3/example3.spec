Name:           example3
BuildArch:      noarch
Version:        1
Release:        1%{?dist}
Summary:        Example 3
License:        GPL
Source:         README.md
Prefix:         %{_prefix}

%description
Just an example

%prep

%build

%install
for x in f1 f2 f3
do
	for y in . a a/b
	do
		mkdir -p $RPM_BUILD_ROOT/%{_datadir}/%{name}/$y
		cp $RPM_BUILD_DIR/README.md $RPM_BUILD_ROOT/%{_datadir}/%{name}/$y/$x
	done
done
mkdir -p $RPM_BUILD_ROOT/%{_datadir}/%{name}/.rpconfig
cp $RPM_BUILD_DIR/manifest.yml $RPM_BUILD_ROOT/%{_datadir}/%{name}/.rpconfig


%clean
rm -rf $RPM_BUILD_ROOT/%{_datadir}

%files
%{_datadir}/%{name}

%changelog
