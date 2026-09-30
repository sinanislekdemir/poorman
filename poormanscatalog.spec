Name:           poormanscatalog
Version:        1.2.1
Release:        1%{?dist}
Summary:        Disk and path catalog creator

License:        GPL-3.0-only
URL:            https://github.com/sinanislekdemir/poorman
Source0:        poorman-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  qt5-qtbase-devel
BuildRequires:  qt5-qttools-devel
BuildRequires:  desktop-file-utils

%description
Poor Man's Catalog creates searchable indexes (catalogs) of disks, external
drives, DVDs and flash drives. Catalogs are stored in SQLite databases, so you
can keep a separate database per disk and search through it later without
plugging the disk back in.

%prep
%autosetup -n poorman-%{version}

%build
qmake-qt5 \
    QMAKE_CFLAGS="%{optflags}" \
    QMAKE_CXXFLAGS="%{optflags}" \
    QMAKE_LFLAGS="%{?__global_ldflags}" \
    -o build/Makefile PoorMansCatalog.pro
make -C build %{?_smp_mflags}

%install
install -Dpm0755 build/PoorMansCatalog %{buildroot}%{_bindir}/poormanscatalog
install -Dpm0644 additional/PoorMansCatalog.desktop \
    %{buildroot}%{_datadir}/applications/poormanscatalog.desktop
install -Dpm0644 additional/icon.png \
    %{buildroot}%{_datadir}/icons/hicolor/256x256/apps/PoorMansCatalog.png
sed -i 's|^Exec=.*|Exec=poormanscatalog|' \
    %{buildroot}%{_datadir}/applications/poormanscatalog.desktop

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/poormanscatalog.desktop

%files
%license LICENSE
%doc README.md
%{_bindir}/poormanscatalog
%{_datadir}/applications/poormanscatalog.desktop
%{_datadir}/icons/hicolor/256x256/apps/PoorMansCatalog.png

%changelog
* Wed Sep 30 2026 Sinan Islekdemir <sinan@islekdemir.com> - 1.2.1-1
- Use Qt's built-in file dialogs to avoid broken native folder picker

* Wed Sep 30 2026 Sinan Islekdemir <sinan@islekdemir.com> - 1.2.0-1
- Initial RPM package
