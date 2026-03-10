configure_file(
  configuration : { 'systemdutildir': systemdutildir },
  input : 'systemd-initctl.service.in',
  output : 'systemd-initctl.service',
  install : true,
  install_dir : systemdsystemunitdir,
)

install_data(
  'systemd-initctl.socket',
  install_dir : systemdsystemunitdir,
)

install_symlink(
  'systemd-initctl.socket',
  install_dir : systemdsystemunitdir / 'sockets.target.wants',
  pointing_to : '../systemd-initctl.socket'
)
