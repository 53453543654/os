# CoraOS File Explorer - Meson build

files_sources = files(
    'main.c',
    'cora-files-window.c',
)

files_deps = [
    dependency('gtk4', version: '>= 4.10'),
    dependency('libadwaita-1', version: '>= 1.3'),
    dependency('glib-2.0', version: '>= 2.74'),
    dependency('gio-2.0'),
    dependency('cairo'),
]

executable('coraos-files',
    files_sources,
    dependencies: files_deps,
    install: true,
    install_dir: get_option('bindir'),
)
