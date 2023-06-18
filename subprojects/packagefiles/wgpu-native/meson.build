
project('wgpu-native', 'c', version : '0.0.0', license : 'MIT OR Apache-2.0')

#build by cargo
run_command(find_program('cargo'), 'build', '--release')

wgpu_native = meson.get_compiler('c').find_library('wgpu_native', dirs: meson.current_source_dir()/'target'/'release')

dep_wgpu_native = declare_dependency(
    dependencies: wgpu_native,
    include_directories: [
        include_directories('ffi', is_system: true),
        include_directories('include', is_system: true) # Just what's under ffi/webgpu-headers in another directory for compatibility purposes.
    ]
)
meson.override_dependency('wgpu-native', dep_wgpu_native)

pkg = import('pkgconfig')
pkg.generate(
    name: 'wgpu-native',
    description: 'Headers for using wgpu-native',
    libraries: [wgpu_native],
    version: meson.project_version(),
)
install_subdir('include', install_dir : '')
