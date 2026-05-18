# CoraOS Shell - Meson build file

shell_sources = files(
    'main.c',
    'cora-shell.c',
    'cora-panel.c',
    'cora-launcher.c',
    'cora-wallpaper.c',
)

shell_deps = [
    dependency('gtk4', version: '>= 4.10'),
    dependency('glib-2.0', version: '>= 2.74'),
    dependency('gio-2.0'),
    dependency('gio-unix-2.0'),
    dependency('json-glib-1.0'),
    dependency('cairo'),
    dependency('pango'),
]

# Try to find libmutter (optional - for full compositor mode)
mutter_dep = dependency('libmutter-13', required: false)
if mutter_dep.found()
    shell_deps += mutter_dep
    add_project_arguments('-DHAVE_MUTTER=1', language: 'c')
endif

executable('coraos-shell',
    shell_sources,
    dependencies: shell_deps,
    install: true,
    install_dir: get_option('bindir'),
)
