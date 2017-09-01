Name: cvmfs-cache-hdfs
Version: 2.0
Release: 1%{?dist}
Summary: HDFS plugin for the CVMFS cache

Group: System Environment/Development
License: BSD
URL: https://github.com/bbockelm/cvmfs-cache-hdfs
# Generated from:
# git archive --format=tgz --prefix=%{name}-%{version}/ v%{version} > %{name}-%{version}.tar.gz
Source0: %{name}-%{version}.tar.gz
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
BuildRequires: cmake
BuildRequires: java7-devel
BuildRequires: jpackage-utils
BuildRequires: hadoop-libhdfs
BuildRequires: openssl-devel
BuildRequires: gcc-c++

# 2.3.99 was a HCC-specific version with the external cache plugin API.
BuildRequires: cvmfs-devel >= 2.3.99

Requires: hadoop-client
Requires: cvmfs >= 2.3.99

%description
%{summary}

%prep
%setup -q

%build
%cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .
make VERBOSE=1 %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_libexecdir}/cvmfs-cache-hdfs/cvmfs-cache-hdfs-plugin
%{_libexecdir}/cvmfs-cache-hdfs/cvmfs-cache-hdfs
%config(noreplace) %{_sysconfdir}/sysconfig/cvmfs-cache-hdfs
%config %{_sysconfdir}/cvmfs/domain.d/osgstorage.org.conf
%config %{_sysconfdir}/cvmfs/config.d/ligo.osgstorage.org.conf
%config %{_sysconfdir}/cvmfs/config.d/cms.osgstorage.org.conf
%config(noreplace) %{_sysconfdir}/cvmfs/cvmfs-cache-hdfs.conf
%config(noreplace) %{_sysconfdir}/logrotate.d/cvmfs-cache-hdfs

%changelog
* Thu Aug 31 2017 Brian Bockelman <bbockelm@cse.unl.edu> - 2.0-1
- Rename all binaries.
- Adopt new naming scheme.

* Sat Aug 05 2017 Brian Bockelman <bbockelm@cse.unl.edu> - 1.3-1
- Fix potential segfault after an IO error.

* Fri Aug 04 2017 Brian Bockelman <bbockelm@cse.unl.edu> - 1.2-1
- Fix reference counting semantics on transaction commit.

* Fri Aug 04 2017 Brian Bockelman <bbockelm@cse.unl.edu> - 1.1-1
- Have plugin drop privileges on startup.

* Thu Aug 03 2017 Brian Bockelman <bbockelm@cse.unl.edu> - 1.0-1
- Initial version of packaging.

