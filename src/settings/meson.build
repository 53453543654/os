# CoraOS Settings - Meson build

settings_sources = files(
    'main.c',
    'cora-settings-window.c',
)

settings_deps = [
    dependency('gtk4', version: '>= 4.10'),
    dependency('libadwaita-1', version: '>= 1.3'),
    dependency('glib-2.0', version: '>= 2.74'),
    dependency('gio-2.0'),
    dependency('json-glib-1.0'),
]

executable('coraos-settings',
    settings_sources,
    dependencies: settings_deps,
    install: true,
    install_dir: get_option('bindir'),
)
