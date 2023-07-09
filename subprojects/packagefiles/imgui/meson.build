project('imgui', 'cpp',
    version: '1.89.6',
    license: 'MIT',
)

include_dirs = include_directories('.')

sources = [
    'imgui_demo.cpp',
    'imgui_draw.cpp',
    'imgui_tables.cpp',
    'imgui_widgets.cpp',
    'imgui.cpp',
]

imgui = static_library('imgui',
    sources,
    cpp_args: meson.get_compiler('cpp').get_supported_arguments(['/W', '-w']),
    include_directories: include_dirs,
)

binary_to_compressed_c = executable('binary_to_compressed_c', 'misc/fonts/binary_to_compressed_c.cpp')
meson.override_find_program('binary_to_compressed_c', binary_to_compressed_c)

imgui_dep = declare_dependency(include_directories: include_dirs, link_with: imgui)
