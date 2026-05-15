Name:           ardium
Version:        2.5.0.2
Release:        1%{?dist}
Summary:        The Ardium Programming Language Compiler

License:        MIT
URL:            https://github.com/dotmini/ardium
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  ocaml
BuildRequires:  dune
BuildRequires:  gcc

%description
Ardium is a high-performance systems programming language designed for
native GUI development and easy C interoperability.

%prep
%setup -q

%build
dune build --profile release

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_bindir}
mkdir -p $RPM_BUILD_ROOT/%{_libdir}/ardium
cp _build/default/bin/main.exe $RPM_BUILD_ROOT/%{_bindir}/arc
cp -r stdlib $RPM_BUILD_ROOT/%{_libdir}/ardium/

%files
%{_bindir}/arc
%{_libdir}/ardium/

%changelog
* Mon Jan 06 2026 Dotmini <dev@dotmini.com> - 2.5.0.2-1
- Initial release
