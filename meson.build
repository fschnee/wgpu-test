project(
    'wgpu-template',
    'cpp',
    meson_version: '>= 0.60.0',
    default_options: [ 'warning_level=3', 'cpp_std=c++20' ]
)

sources = [
    'src/ghuva/utils/point.cpp',
    'src/ghuva/context.cpp',

    'src/imgui_widgets/group_panel.cpp',
    'src/webgpu.cpp',
    'src/main.cpp',
    'src/zep.cpp'
]

dependencies = [
    dependency('wgpu-cpp'),
    dependency('openmp'),
    dependency('imgui',    version: '>= 1.89.6', fallback: ['imgui', 'imgui_dep']).as_system('system'),
    dependency('rapidobj', version: '= 1.0.1',   fallback: ['rapidobj', 'rapidobj_dep']).as_system('system'),
    dependency('fmt',      version: '>= 7.0.0',  fallback: ['fmt', 'fmt_dep']).as_system('system'),
    dependency('zep',                            fallback: ['zep', 'zep_dep']).as_system('system'),
    meson.is_cross_build() ? [] : [
        dependency('wgpu-native'),
        dependency('glfw3'),
        dependency('glfw3webgpu'), # From https://eliemichel.github.io/LearnWebGPU/getting-started/the-adapter.html.
                                   # Just changed the included headers.
    ],
]
incdirs = include_directories('src')

executable('main', sources, dependencies: dependencies, include_directories: incdirs)
if meson.is_cross_build()
    configure_file(input: 'src/main.html', output: 'main.html', copy: true)
endif