# CoraOS WiFi Manager - Meson build

wifi_sources = files(
    'main.c',
    'cora-wifi-window.c',
)

wifi_deps = [
    dependency('gtk4', version: '>= 4.10'),
    dependency('libadwaita-1', version: '>= 1.3'),
    dependency('glib-2.0', version: '>= 2.74'),
    dependency('gio-2.0'),
    dependency('libnm', version: '>= 1.40'),
]

executable('coraos-wifi',
    wifi_sources,
    dependencies: wifi_deps,
    install: true,
    install_dir: get_option('bindir'),
)
