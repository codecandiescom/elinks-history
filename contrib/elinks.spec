# This is RPM spec file for ELinks. This file is currently not officially
# maintained, so the informations here are probably totally obsolete, and you
# should update them before making a package.
#
# Note also that most of the variable stuff should be generated from automake/
# autoconf somehow. As I don't use this file personally, I'm too lazy to
# research a way how to do it, I'll gladly receive a patch for that, however.
#
# --pasky
#
# This file was contributed by <zimon@niksula.hut.fi>.
#
# Spec file format from < http://www.rpm.org/RPM-HOWTO/build.html >

%define name elinks
%define dirname elinks
%define version 0.4pre4
%define versiondate 20020328
%define release 1
%define prefix /usr
%define manpath %{prefix}/share/man
%define docdir %{prefix}/share/doc
#%define subdir elinks-0.4pre4-20020328

%define deprecated_name links
%define program_name elinks

%define builddir $RPM_BUILD_DIR/%{dirname}-%{version}-%{versiondate}
#%define builddir2 $RPM_BUILD_DIR/%{subdir}

Summary: Development version of Links (Lynx-like text WWW browser)
Name: %{name}
Version: %{version}
Release: %{release}
Prefix: %{prefix}
Copyright: GPL
Vendor: Elinks project <pasky@ji.cz>
#Packager: Petr Baudis <pasky@ji.cz>
Group: Applications/Internet
Source: http://pasky.ji.cz/elinks/%{name}-%{version}.tar.bz2
URL: http://pasky.ji.cz/elinks/
BuildRoot: /tmp/%{name}-%{version}-root


%description
Enhanced version of Links (Lynx-like text WWW browser), with more liberal
features policy and development style.

This is the ELinks - intended to provide feature-rich version of links.
Its purpuose is to make alternative to links, and to test and tune various
patches for Mikulas to be able to include them in the official links
releases.

%prep
rm -rf %{builddir}

%setup -n %{builddir}

touch `find . -type f`

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix}
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/%{manpath}/man1
make DESTDIR=$RPM_BUILD_ROOT install

# Eh?? --pasky

cp contrib/README contrib_README
cp contrib/hooks.lua hooks.lua
cp Unicode/README Unicode_README
cp intl/README intl_README
cp src/README src_README
bzip2 -9 AUTHORS BUGS ChangeLog* COPYING HACKING LUA README SITES TODO contrib_README hooks.lua Unicode_README intl_README src_README

cd $RPM_BUILD_ROOT
mv  ./%{prefix}/bin/%{deprecated_name} ./%{prefix}/bin/%{program_name}
mv  ./%{prefix}/man/man1/%{deprecated_name}.1 ./%{manpath}/man1/%{program_name}.1

%clean
rm -rf $RPM_BUILD_ROOT
rm -rf %{builddir}

%files 
%doc AUTHORS* BUGS* ChangeLog* COPYING* HACKING* LUA* README* SITES* TODO* contrib_README* Unicode_README* intl_README* src_README* hooks.lua*
%{prefix}/bin/%{program_name}
%{manpath}/man1/%{program_name}.1.gz

# date +"%a %b %d %Y"
%changelog
* Thu Apr 04 2002 pasky@ji.cz
- Changed some stuff so that it's now ready for inclusion..

* Sat Mar 16 2002 zimon (ät) iki fi
- Made my own elinks.spec file as the one I found with Google didn't work how
  I wanted

