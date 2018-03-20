# Copyright 2018 Endless Mobile, Inc.

version = configuration_data()
version.set('EKNR_MAJOR_VERSION', version_components[0])
version.set('EKNR_MINOR_VERSION', version_components[1])
version.set('EKNR_MICRO_VERSION', version_components[2])
version_h = configure_file(configuration: version,
    input: 'eknr-version.h.in', output: 'eknr-version.h')

installed_headers = [
    'eknr.h',
    version_h,
    'eknr-errors.h',
    'eknr-renderer.h'
]
sources = [
    'eknr-errors.c',
    'eknr-renderer.c',
]

include = include_directories('..')

enum_sources = gnome.mkenums_simple('eknr-enums', install_header: true,
    install_dir: join_paths(get_option('includedir'), api_name),
    sources: installed_headers)

main_library = library('@0@-@1@'.format(meson.project_name(), api_version),
    enum_sources, sources, installed_headers,
    c_args: ['-DG_LOG_DOMAIN="@0@"'.format(namespace_name),
        '-DCOMPILING_EKNR'],
    dependencies: [gio, glib, gobject, json_glib, libendless, mustache],
    include_directories: include, install: true,
    link_depends: 'lib@0@.map'.format(meson.project_name()),
    soversion: api_version, version: libtool_version)

introspection_sources = [sources, enum_sources, installed_headers]

gnome.generate_gir(
    main_library,
    extra_args: ['--warn-all', '--warn-error', '-DCOMPILING_EKNR'],
    identifier_prefix: 'Eknr', include_directories: include,
    includes: ['Endless-0', 'Gio-2.0', 'GLib-2.0', 'GObject-2.0', 'Json-1.0'],
    install: true, namespace: namespace_name, nsversion: api_version,
    sources: introspection_sources, symbol_prefix: 'eknr',
)

install_headers(installed_headers, subdir: api_name)
