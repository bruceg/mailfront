Name: @PACKAGE@
Summary: Mail server network protocol front-ends
Version: @VERSION@
Release: 1
Copyright: GPL
Group: Utilities/System
Source: http://untroubled.org/@PACKAGE@/@PACKAGE@-@VERSION@.tar.gz
BuildRoot: %{_tmppath}/@PACKAGE@-buildroot
BuildRequires: bglibs >= 1.001
URL: http://untroubled.org/@PACKAGE@/
Packager: Bruce Guenter <bruceg@em.ca>

%description
This is mailfront, a package containing customizeable network front-ends
for mail servers.  Right now the only protocol that is handled is SMTP.

%prep
%setup
echo "gcc %{optflags} -I/usr/local/bglibs/include" >conf-cc
echo "gcc -s -L/usr/local/bglibs/lib" >conf-ld
echo %{_bindir} >conf-bin

%build
make

%install
rm -fr %{buildroot}
rm -f conf_bin.c insthier.o installer instcheck
echo %{buildroot}%{_bindir} >conf-bin
make installer instcheck

mkdir -p %{buildroot}%{_bindir}
./installer
./instcheck

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc ANNOUNCEMENT COPYING NEWS README *.html
%{_bindir}/*
