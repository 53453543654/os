# CoraOS Browser - Meson build

browser_sources = files(
    'main.c',
    'cora-browser-window.c',
)

# Try webkitgtk-6.0 first (Fedora 40+), fall back to webkit2gtk-5.0
webkit_browser_dep = dependency('webkitgtk-6.0', required: false)
if not webkit_browser_dep.found()
    webkit_browser_dep = dependency('webkit2gtk-5.0', required: true)
endif

browser_deps = [
    dependency('gtk4', version: '>= 4.10'),
    dependency('libadwaita-1', version: '>= 1.3'),
    dependency('glib-2.0', version: '>= 2.74'),
    dependency('gio-2.0'),
    webkit_browser_dep,
]

executable('coraos-browser',
    browser_sources,
    dependencies: browser_deps,
    install: true,
    install_dir: get_option('bindir'),
)
