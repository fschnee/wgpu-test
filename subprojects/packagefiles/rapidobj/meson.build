project('rapidobj', 'cpp', version : '1.0.1')

rapidobj_dep = declare_dependency(
    include_directories: include_directories('include'),
    dependencies: dependency('threads') # Export this dep to whoever includes me.
)
meson.override_dependency('rapidobj', rapidobj_dep)

