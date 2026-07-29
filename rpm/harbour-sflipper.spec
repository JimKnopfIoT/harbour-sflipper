# Neutral packaging metadata — no personal identifiers (see anonymity rules).
# The build host's name would otherwise end up in the RPM header.
%define _buildhost reproducible-builder

Name:       harbour-sflipper
Summary:    Native Flipper Zero client for Sailfish OS
Version:    0.1.0
Release:    1
License:    GPL-3.0-or-later
URL:        https://github.com/JimKnopfIoT/harbour-sflipper
Source0:    %{name}-%{version}.tar.bz2
Vendor:     harbour-sflipper contributors
Packager:   harbour-sflipper contributors

Requires:   sailfishsilica-qt5
BuildRequires: pkgconfig(sailfishapp)
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Qml)
BuildRequires: pkgconfig(Qt5Quick)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: desktop-file-utils

%description
Unofficial native Sailfish OS client for the Flipper Zero. Speaks the Flipper
RPC (protobuf) protocol over BLE (BlueZ GATT) and USB-C (CDC-ACM): device info
and battery, live screen mirroring with on-screen remote control, an SD-card
file manager, triggering saved Sub-GHz / Infrared / NFC / RFID signals,
renaming the device, and installing firmware updates — all without Android
App Support.

%prep
%setup -q

%build
%qmake5
%make_build

%install
%qmake5_install

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%config %{_sysconfdir}/udev/rules.d/99-flipper.rules

%changelog
* Wed Jul 29 2026 harbour-sflipper contributors 0.1.0-1
- First public release: USB-C + BLE transport, device info/battery, screen
  mirror + remote control with touch gestures, storage file manager, saved
  signal trigger, device rename, and firmware update (trigger + upload).
