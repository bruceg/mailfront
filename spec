Name: @PACKAGE@
Summary: Mail server network protocol front-ends
Version: @VERSION@
Release: 1
License: GPL
Group: Utilities/System
Source: http://untroubled.org/@PACKAGE@/@PACKAGE@-@VERSION@.tar.gz
BuildRoot: %{_tmppath}/@PACKAGE@-buildroot
BuildRequires: bglibs >= 1.101
BuildRequires: cvm-devel >= 0.81
URL: http://untroubled.org/@PACKAGE@/
Packager: Bruce Guenter <bruceg@em.ca>

%description
This is mailfront, a package containing customizeable network front-ends
for mail servers.  Handles POP3, QMQP, QMTP, SMTP, and IMAP
(authentication only).

%package devel
Summary: Mailfront development bits
Group: Development/Libraries
%description devel
Headers for building modules (front-ends, plugins, and back-ends) for
mailfront.

%prep
%setup
echo "gcc %{optflags}" >conf-cc
echo "gcc %{optflags} -fPIC -shared" >conf-ccso
echo "gcc -s -rdynamic" >conf-ld
echo %{_bindir} >conf-bin
echo %{_libdir}/mailfront >conf-modules
echo %{_includedir} >conf-include

%build
make

%install
rm -fr %{buildroot}
make install_prefix=%{buildroot} install

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc ANNOUNCEMENT COPYING NEWS README *.html
%{_bindir}/*
%{_libdir}/mailfront

%files devel
%{_includedir}/mailfront
