# Copyright 2018 Endless Mobile, Inc.
#
# We only need to have this file in this subdirectory
# as outputs to custom_target cannot have path separators
# their names.

# Call find_program here, since we want to make sure that sassc is
# installed
sassc = find_program('sassc')
meson_sassc = find_program('./meson-sassc')

scss_sources = [
    'clipboard.scss',
    'exploration-custom.scss',
    'share-actions.scss',
    'wikihow.scss',
    'wikimedia.scss',
]

scss_generated_targets = []
foreach scss_source : scss_sources
    scss_generated_targets += custom_target(
        'generate-template-scss-' + scss_source,
        input: scss_source,
        output: scss_source.split('.')[0] + '.css',
        depfile: 'sassc.deps',
        command: [meson_sassc, sassc, '@INPUT@', '@OUTPUT@', '@DEPFILE@']
    )
endforeach

