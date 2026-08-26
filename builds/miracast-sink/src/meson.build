project('miracast-sink', 'c',
  version: '0.3.0',
  license: 'LGPL-2.1+',
  meson_version: '>= 1.0',
  default_options: ['c_std=c11', 'warning_level=2', 'buildtype=release'])

gnome = import('gnome')
i18n = import('i18n')

gtk4_dep = dependency('gtk4', version: '>= 4.10')
adwaita_dep = dependency('libadwaita-1', version: '>= 1.4')
gst_dep = dependency('gstreamer-1.0', version: '>= 1.20')
gst_base_dep = dependency('gstreamer-base-1.0', version: '>= 1.20')

# Chromecast dependencies
avahi_client_dep = dependency('avahi-client', required: false)
avahi_glib_dep = dependency('avahi-glib', required: false)
protobuf_c_dep = dependency('libprotobuf-c', required: false)
json_glib_dep = dependency('json-glib-1.0', required: false)

# Build backend as a static library
backend_sources = files(
  'backend/msk-backend.c',
  'backend/miracast/msk-miracast-backend.c',
  'backend/miracast/dbus-wifi.c',
  'backend/miracast/rtsp-wfd.c',
)

backend_deps = [
  gtk4_dep,
  adwaita_dep,
  gst_dep,
  gst_base_dep,
]

# Add Chromecast deps if available
if avahi_client_dep.found() and avahi_glib_dep.found() and protobuf_c_dep.found() and json_glib_dep.found()
  backend_sources += files(
    'backend/chromecast/msk-chromecast-backend.c',
    'backend/chromecast/discovery.c',
    'backend/chromecast/client.c',
    'backend/chromecast/media.c',
    'backend/chromecast/cast_channel.pb-c.c',
  )
  backend_deps += [
    avahi_client_dep,
    avahi_glib_dep,
    protobuf_c_dep,
    json_glib_dep,
  ]
  add_project_arguments('-DHAVE_CHROMECAST', language: 'c')
  message('Chromecast backend: enabled')
else
  message('Chromecast backend: disabled (missing dependencies)')
endif

backend_lib = static_library('backend',
  backend_sources,
  dependencies: backend_deps,
)

sources = files(
  'main.c',
  'window.c',
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
  link_with: backend_lib,
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
  'data' / 'msk-wifid.service',
  install_dir: get_option('libdir') / 'systemd' / 'system')
