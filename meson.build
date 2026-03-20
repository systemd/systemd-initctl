project('systemd-initctl', 'c',
  version : '4',
  meson_version : '>=0.61.0'
)

libsystemd = dependency('libsystemd')

systemd = dependency('systemd')
systemdsystemunitdir = systemd.get_variable('systemdsystemunitdir')
systemdutildir = systemd.get_variable('systemdutildir')

subdir('src')
subdir('units')
