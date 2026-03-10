project(
  'systemd-initctl',
  'c',
  version : '2',
)

libsystemd = dependency('libsystemd')

systemd = dependency('systemd')
systemdsystemunitdir = systemd.get_variable('systemdsystemunitdir')
systemdutildir = systemd.get_variable('systemdutildir')

subdir('src')
subdir('units')
