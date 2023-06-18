
project('glfw3webgpu', 'c', version : '0.0.0',)

incdirs = include_directories('.', is_system: true)

glfw3webgpu_lib = library(
  'glfw3webgpu',
  'glfw3webgpu.c',
  include_directories: incdirs,
  dependencies: [dependency('glfw3'), dependency('wgpu-native')],
)

glfw3webgpu_dep = declare_dependency(
    link_with: glfw3webgpu_lib,
    include_directories: incdirs
)
meson.override_dependency('glfw3webgpu', glfw3webgpu_dep)
