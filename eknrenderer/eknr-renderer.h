/* Copyright 2018 Endless Mobile, Inc. */

#pragma once

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define EKNR_TYPE_RENDERER eknr_renderer_get_type ()
G_DECLARE_FINAL_TYPE (EknrRenderer, eknr_renderer, EKNR, RENDERER, GObject)

char * eknr_renderer_render_mustache_document (EknrRenderer *renderer,
                                               const char   *tmpl_text,
                                               GVariant     *variables,
                                               GError      **error);

char * eknr_renderer_render_mustache_document_from_file (EknrRenderer *renderer,
                                                         GFile        *file,
                                                         GVariant     *variables,
                                                         GError      **error);

char * eknr_renderer_render_legacy_content (EknrRenderer  *renderer,
                                            const char    *body_html,
                                            const char    *source,
                                            const char    *source_name,
                                            const char    *original_uri,
                                            const char    *license,
                                            const char    *title,
                                            gboolean       show_title,
                                            gboolean       use_scroll_manager,
                                            GError       **error);

EknrRenderer * eknr_renderer_new (void);

G_END_DECLS
