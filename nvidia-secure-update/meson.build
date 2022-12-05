cppfs = meson.get_compiler('cpp').find_library('stdc++fs')
lib_headers = ['.', '../']

ssl = dependency('openssl')

gpiodcxx = dependency(
     'libgpiodcxx',
     fallback: ['libgpiod', 'gpiodcxx_dep'],
     default_options: ['bindings=cxx'],
)
libgpiodcxx = dependency('libgpiodcxx')
sdeventplus = dependency('sdeventplus') 

nvidia_secure_monitor_sources = files(
    'i2c.cpp',
    'i2c_comm_lib.cpp',
    'secure_monitor.cpp',
    '../openssl_alloc.cpp'
)

nvidia_fw_updater_sources = files(
    'state_machine_context.cpp',
    'state_machine.cpp',
    'i2c.cpp',
    'i2c_comm_lib.cpp',
    'pris_ap_fw_state_machine.cpp',
    'ap_fw_updater.cpp',
    'pris_state_machine.cpp', 
    'ap_fw_updater_main.cpp',
    '../watch.cpp',
    '../openssl_alloc.cpp'
)

nvidia_secure_copier_sources = files(
    'i2c.cpp',
    'i2c_comm_lib.cpp',
    'nvidia_copy_image.cpp',
    '../openssl_alloc.cpp'
)

nvidia_fw_tool_sources = files(
    'i2c.cpp',
    'i2c_comm_lib.cpp',
    'nvidia_fw_services.cpp',
    '../openssl_alloc.cpp'
)

executable(
    'nvidia-secure-monitor',
    nvidia_secure_monitor_sources,
    include_directories: include_directories(lib_headers),
    dependencies: [cppfs, deps, ssl, gpiodcxx, sdeventplus],
    install: true
)

install_data('obmc-secure-copy-image',
    install_mode: 'rwxr-xr-x',
    install_dir: get_option('bindir')
)

executable(
    'nvidia-fw-updater',
    nvidia_fw_updater_sources,
    include_directories: include_directories(lib_headers),
    dependencies: [cppfs, deps, ssl, sdeventplus],
    install: true
)

executable(
    'nvidia-secure-copier',
    nvidia_secure_copier_sources,
    include_directories: include_directories(lib_headers),
    dependencies: [cppfs, deps, ssl, sdeventplus],
    install: true
)

executable(
    'nvidia-fw-tool',
    nvidia_fw_tool_sources,
    include_directories: include_directories(lib_headers),
    dependencies: [cppfs, deps, ssl, sdeventplus],
    install: true
)
