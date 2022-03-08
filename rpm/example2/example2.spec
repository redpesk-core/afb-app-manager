Name:           example2
BuildArch:      noarch
Version:        1
Release:        1%{?dist}
Summary:        Example 2
License:        GPL
Source:         README.md
Prefix:         %{_prefix}

%description
Just an example

%prep

%build

%install
mkdir -p $RPM_BUILD_ROOT/%{_datadir}/%{name}/a/b
for x in f1 f2 f3
do
	for y in . a a/b
	do
		cp $RPM_BUILD_DIR/README.md $RPM_BUILD_ROOT/%{_datadir}/%{name}/$y/$x
	done
done

%clean
rm -rf $RPM_BUILD_ROOT/%{_datadir}

%files
%{_datadir}/%{name}

%changelog
