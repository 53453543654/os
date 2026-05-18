# CoraOS Browser - Meson build

browser_sources = files(
    'main.c',
    'cora-browser-window.c',
)

browser_deps = [
    dependency('gtk4', version: '>= 4.10'),
    dependency('libadwaita-1', version: '>= 1.3'),
    dependency('glib-2.0', version: '>= 2.74'),
    dependency('gio-2.0'),
    dependency('webkit2gtk-5.0'),
]

executable('coraos-browser',
    browser_sources,
    dependencies: browser_deps,
    install: true,
    install_dir: get_option('bindir'),
)
