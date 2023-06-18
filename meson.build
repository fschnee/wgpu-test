project(
    'wgpu-template',
    ['cpp', 'rust'],
    meson_version: '>= 0.59.0',
    default_options: [ 'warning_level=3', 'cpp_std=c++20' ]
)

sources = ['src/main.cpp']
dependencies = [
    dependency('wgpu-native'),
    dependency('wgpu-cpp'),
    dependency('glfw3'),
    dependency('glfw3webgpu'), # From https://eliemichel.github.io/LearnWebGPU/getting-started/the-adapter.html.
                               # Just changed the included headers.
]
incdirs = include_directories('src')

executable('main', sources, dependencies: dependencies, include_directories: incdirs)