project('miracast-sink', 'c',
  version: '0.1.0',
  license: 'LGPL-2.1+',
  meson_version: '>= 1.0',
  default_options: ['c_std=c11', 'warning_level=2', 'buildtype=release'])

gnome = import('gnome')
i18n = import('i18n')

gtk4_dep = dependency('gtk4', version: '>= 4.10')
adwaita_dep = dependency('libadwaita-1', version: '>= 1.4')
gst_dep = dependency('gstreamer-1.0', version: '>= 1.20')
gst_base_dep = dependency('gstreamer-base-1.0', version: '>= 1.20')

sources = files(
  'main.c',
  'window.c',
  'dbus-wifi.c',
  'rtsp-wfd.c',
  'stream.c',
)

deps = [
  gtk4_dep,
  adwaita_dep,
  gst_dep,
  gst_base_dep,
]

executable('miracast-sink',
  sources,
  dependencies: deps,
  install: true,
  install_dir: get_option('bindir'))

install_data(
  'data' / 'org.miracast.sink.desktop',
  install_dir: get_option('datadir') / 'applications')

install_data(
  'data' / 'org.freedesktop.miracle.conf',
  install_dir: get_option('sysconfdir') / 'dbus-1' / 'system.d')

install_data(
  'data' / 'miracast-wifid.service',
  install_dir: get_option('libdir') / 'systemd' / 'system')