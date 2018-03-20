# Copyright 2018 Endless Mobile, Inc.

project('eknr', 'c', version: '0.0.0', license: 'LGPL2.1+',
    meson_version: '>= 0.42.0',
    default_options: ['c_std=c99', 'warning_level=3'])

gnome = import('gnome')
pkg = import('pkgconfig')

version_components = meson.project_version().split('.')
api_version = version_components[0]
api_name = '@0@-@1@'.format(meson.project_name(), api_version)
libtool_version = '0.0.0'
namespace_name = 'Eknr'

# Build options

mathjax_dir = get_option('mathjax_dir')

# Dependencies

libendless_req = '>= 0.5'

gio = dependency('gio-2.0')
glib = dependency('glib-2.0')
gobject = dependency('gobject-2.0')
libendless = dependency('endless-0', version: libendless_req)
json_glib = dependency('json-glib-1.0')
mustache = dependency('mustache_c-1.0')

# Data files

subdir('data/templates/css')

# Resources

gresources = gnome.compile_resources(
    'eknr-resources',
    'eknr.gresource.xml',
    dependencies: scss_generated_targets,
    source_dir: [meson.current_source_dir(), meson.current_build_dir()],
    c_name: 'eknr'
)

subdir('eknrenderer')

# Generated Files

config = configuration_data()
config.set_quoted('EKNR_VERSION', meson.project_version())
config.set_quoted('MATHJAX_PATH', mathjax_dir)
configure_file(configuration: config, output: 'config.h')

requires = ['glib-2.0', 'gio-2.0', 'gobject-2.0']
requires_private = [
    'endless-0 @0@'.format(libendless_req),
    'json-glib-1.0'
]
pkg.generate(filebase: api_name, libraries: [main_library],
    description: 'Legacy content renderer library for Endless OS offline content.',
    name: meson.project_name(), subdirs: api_name, requires: requires,
    requires_private: requires_private,
    url: 'http://endlessm.github.io/eos-knowledge-content-renderer',
    version: meson.project_version())

# Summary

message('\n'.join([
    '@0@ @1@ (@2@)'.format(meson.project_name(), meson.project_version(),
        api_name),
    '-------------------',
    'Options:',
    '  Mathjax Directory: @0@'.format(mathjax_dir),
    '',
    'Directories:',
    '    Install prefix: @0@'.format(get_option('prefix')),
    ''
]))
