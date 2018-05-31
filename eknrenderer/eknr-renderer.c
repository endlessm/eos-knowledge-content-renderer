/* Copyright 2018 Endless Mobile, Inc. */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <endless/endless.h>
#include <json-glib/json-glib.h>
#include <mustache.h>

#include "eknr-errors.h"
#include "eknr-renderer.h"

#include <glib/gi18n-lib.h>

/**
 * SECTION:renderer
 * @title: Renderer
 * @short_description: A post-processing renderer for HTML article content
 *
 * The renderer is responsible for adding some final postprocessing to
 * articles on the client side before it is displayed to the user. What
 * postprocessing happens depends on the article's source.
 */
struct _EknrRenderer
{
  GObject parent_instance;
};

typedef struct _EknrRendererPrivate
{
  GHashTable *cache; /* key-type=char *, char * */
} EknrRendererPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EknrRenderer,
                            eknr_renderer,
                            G_TYPE_OBJECT)

/* This struct is the "closure" that we usually pass to
 * mustache so that we can keep track of some data as it
 * gets passed to callbacks that we register with mustache.
 *
 * Notably, this struct is used for input *and* output - when
 * a value is to be written mustache will call _renderer_write_to_closure
 * where we write the output to the "output" member.
 *
 * Also note that we also get to define our own error handler - in this
 * case we keep an error-out location in the "error" member of the
 * struct which gets written to in case something fails. Importantly,
 * that means that this closure is *not* async-safe if you intend to pass
 * a location of an error pointer on the stack.
 */
typedef struct _RendererMustacheData {
  GVariantDict  *variables;
  GError       **error; /* non-owned */

  mustache_str_ctx input;
  mustache_str_ctx output;

  const char *section_variable;
} RendererMustacheData;

static RendererMustacheData *
renderer_mustache_data_new (GVariantDict  *variables,
                            GError       **error,
                            const char    *input)
{
  RendererMustacheData *data = g_new0 (RendererMustacheData, 1);
  data->variables = variables ? g_variant_dict_ref (variables) : NULL;
  data->error = error;
  data->input.string = input ? g_strdup (input) : NULL;
  data->input.offset = 0;
  data->output.string = NULL;
  data->output.offset = 0;

  return data;
}

static void
renderer_mustache_data_free (RendererMustacheData *data)
{
  g_clear_pointer (&data->variables, g_variant_dict_unref);
  g_clear_pointer (&data->output.string, free);
  g_clear_pointer (&data->input.string, g_free);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (RendererMustacheData,
                               renderer_mustache_data_free)

static void
free_mustache_template (mustache_template_t *template)
{
  mustache_api_t api = {
    .freedata = NULL
  };

  mustache_free (&api, template);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (mustache_template_t, free_mustache_template)

static uintmax_t
_renderer_read_from_closure (mustache_api_t *api,
                             void           *userdata,
                             char           *buffer,
                             uintmax_t       buffer_size)
{
  RendererMustacheData *data = userdata;
  return mustache_std_strread (api, &data->input, buffer, buffer_size);
}

static uintmax_t
_renderer_write_to_closure (mustache_api_t *api,
                            void           *userdata,
                            const char     *buffer,
                            uintmax_t       buffer_size)
{
  RendererMustacheData *data = userdata;

  /* 'buffer' here is actually a read-only buffer, but we have to cast it to
   * (char *) because that's the way that mustache_std_strwrite was declared. */
  return mustache_std_strwrite (api, &data->output, (char *) buffer, buffer_size);
}

static const char *
_lookup_in_gvariant_dict (RendererMustacheData *data,
                          const char           *text,
                          mustache_api_t       *api)
{
  g_autoptr(GVariant) value_v = g_variant_dict_lookup_value (data->variables, text, G_VARIANT_TYPE_STRING);

  if (value_v == NULL)
    {
      g_autofree char *msg = g_strdup_printf ("No such variable %s", text);
      (*api->error) (api, data, __LINE__, msg);
      return NULL;
    }

  return g_variant_get_string (value_v, NULL);
}

/**
 * maybe_escape_value:
 * @value: The string to escape
 * @is_escaped: Whether or not to apply escaping
 *
 * Based on what mustache.js does to escape HTML.
 *
 * Returns: (transfer full): an escaped value.
 */
static char *
maybe_escape_value (const char     *value,
                    gboolean        is_escaped)
{
  if (!is_escaped)
    return g_strdup (value);

  return g_markup_escape_text (value, -1);
}

static uintmax_t
_renderer_var_from_ht (mustache_api_t            *api,
                       void                      *userdata,
                       mustache_token_variable_t *token)
{
  RendererMustacheData *data = userdata;
  const char *value = NULL;
  g_autofree char *maybe_escaped_value = NULL;

  /* First, if we're in a section in the value is ".", then we
   * need to replace it with the section name. */
  if (data->section_variable != NULL && token->text[0] == '.')
    value = data->section_variable;
  else
    value = _lookup_in_gvariant_dict (data, token->text, api);

  if (value == NULL)
    return 0;

  /* Take ownership here */
  maybe_escaped_value = maybe_escape_value (value,
                                            token->escaped == 1);

  (*api->write) (api,
                 userdata,
                 maybe_escaped_value,
                 strlen (maybe_escaped_value));
  return 1;
}

static uintmax_t
_renderer_strv_sect_from_ht (mustache_api_t           *api,
                             void                     *userdata,
                             mustache_token_section_t *token,
                             GVariant                 *variant)
{
  RendererMustacheData *data = userdata;
  g_autofree const char **strv = g_variant_get_strv (variant, NULL);
  const char **iter = strv;

  /* Keep track of the current section variable, then pop it once we're
   * done */
  const char *last_section_variable = data->section_variable;

  /* Need to render each sub-template from here */
  for (; *iter != NULL; ++iter)
    {
      data->section_variable = *iter;

      if (!mustache_render (api, userdata, token->section))
        {
          data->section_variable = last_section_variable;
          return 0;
        }
    }

  data->section_variable = last_section_variable;
  return 1;
}

static uintmax_t
_renderer_bool_sect_from_ht (mustache_api_t           *api,
                             void                     *userdata,
                             mustache_token_section_t *token,
                             GVariant                 *variant)
{
  /* Need to render each sub-template from here */
  if (g_variant_get_boolean (variant))
    return mustache_render (api, userdata, token->section);

  /* Nothing to do */
  return 1;
}

static uintmax_t
_renderer_str_sect_from_ht (mustache_api_t           *api,
                            void                     *userdata,
                            mustache_token_section_t *token,
                            GVariant                 *variant)
{
  RendererMustacheData *data = userdata;
  uintmax_t rv = 0;
  const char *last_section_variable = data->section_variable;

  data->section_variable = g_variant_get_string (variant, NULL);

  /* Need to render each sub-template from here */
  rv = mustache_render (api, userdata, token->section);

  data->section_variable = last_section_variable;

  return rv;
}

static uintmax_t
_renderer_section_from_ht_variant (GVariant                 *variant,
                                   mustache_api_t           *api,
                                   void                     *userdata,
                                   mustache_token_section_t *token)
{
  g_autofree char *msg = NULL;

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING_ARRAY))
    return _renderer_strv_sect_from_ht (api, userdata, token, variant);
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_BOOLEAN))
    return _renderer_bool_sect_from_ht (api, userdata, token, variant);
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING))
    return _renderer_str_sect_from_ht (api, userdata, token, variant);

  msg = g_strdup_printf ("No handler for section type %s on token %s",
                         g_variant_get_type_string (variant),
                         token->name);
  (*api->error) (api, userdata, __LINE__, msg);
  return 1;
}

static uintmax_t
_renderer_sect_from_ht (mustache_api_t           *api,
                        void                     *userdata,
                        mustache_token_section_t *token)
{
  RendererMustacheData *data = userdata;
  g_autoptr(GVariant) value = g_variant_dict_lookup_value (data->variables,
                                                           token->name,
                                                           NULL);

  if (value == NULL)
    {
      g_autofree char *msg = g_strdup_printf ("No such section %s", token->name);
      (*api->error) (api, data, __LINE__, msg);
      return 0;
    }

  if (!_renderer_section_from_ht_variant (value, api, userdata, token))
    return 0;

  return 1;
}

static void
_renderer_set_error (G_GNUC_UNUSED mustache_api_t *api,
                     void                         *userdata,
                     uintmax_t                     lineno,
                     const char                   *msg)
{
  RendererMustacheData *data = userdata;

  g_set_error (data->error,
               EKNR_ERROR,
               EKNR_ERROR_SUBSTITUTION_FAILED,
               "Failed to perform template substitution: %s (at line %lu)",
               msg,
               lineno);
}

static mustache_api_t _renderer_mustache_data_vfuncs = {
  .read = &_renderer_read_from_closure,
  .write = &_renderer_write_to_closure,
  .varget = &_renderer_var_from_ht,
  .sectget = &_renderer_sect_from_ht,
  .error = &_renderer_set_error
};

static char *
_renderer_render_mustache_document_internal (mustache_template_t  *tmpl,
                                             GVariant             *variables,
                                             GError              **error)
{
  g_autoptr(GVariantDict) variables_dict = g_variant_dict_new (variables);
  g_autoptr(RendererMustacheData) data = renderer_mustache_data_new (variables_dict,
                                                                     error,
                                                                     NULL);

  if (!mustache_render (&_renderer_mustache_data_vfuncs, data, tmpl))
    return NULL;

  return g_steal_pointer (&data->output.string);
}

/**
 * eknr_renderer_render_mustache_document_from_file:
 * @renderer: An #EknrRenderer
 * @file: A #GFile specifying the location of the template file
 * @variables: The variables and sections to use when rendering.
 * @error: A #GError
 *
 * Use mustache_c to render a document, similar to
 * eknr_renderer_render_mustache_document, but read the template
 * from the file specified at @file. If that file has already been
 * read, its contents will be read from the internal cache.
 *
 * Returns: (transfer full): The renderered document on success, %NULL on error.
 */
char *
eknr_renderer_render_mustache_document_from_file (EknrRenderer *renderer,
                                                  GFile        *file,
                                                  GVariant     *variables,
                                                  GError      **error)
{
  g_autofree char *uri = g_file_get_uri (file);
  EknrRendererPrivate *priv = eknr_renderer_get_instance_private (renderer);
  mustache_template_t *tmpl = g_hash_table_lookup (priv->cache, uri);
  g_autofree char *contents = NULL;
  g_autoptr(RendererMustacheData) data = NULL;

  if (tmpl != NULL)
    return _renderer_render_mustache_document_internal (tmpl, variables, error);

  if (!g_file_load_contents (file, NULL, &contents, NULL, NULL, error))
    return NULL;

  data = renderer_mustache_data_new (NULL,
                                     error,
                                     contents);
  tmpl = mustache_compile (&_renderer_mustache_data_vfuncs, data);

  if (tmpl == NULL)
    return NULL;

  g_hash_table_replace (priv->cache, g_steal_pointer (&uri), tmpl);

  return _renderer_render_mustache_document_internal (tmpl, variables, error);
}

/**
 * eknr_renderer_render_mustache_document:
 * @renderer: An #EknrRenderer
 * @tmpl_text: The template to render
 * @variables: The variables and sections to use when rendering.
 * @error: A #GError
 *
 * Use mustache_c to render a document. The provided @variables variant
 * is used as substitutions. The variant should be of type
 * 'a{sv}' and each child node should be either 's' or a variable subsitution
 * 'as' for a section substitution.
 *
 * Returns: (transfer full): The renderered document on success, %NULL on error.
 */
char *
eknr_renderer_render_mustache_document (EknrRenderer  *renderer,
                                        const char    *tmpl_text,
                                        GVariant      *variables,
                                        GError       **error)
{
  g_return_val_if_fail (renderer && EKNR_IS_RENDERER (renderer), NULL);

  g_autoptr(GVariantDict) variables_dict = g_variant_dict_new (variables);
  g_autoptr(RendererMustacheData) data = renderer_mustache_data_new (variables_dict,
                                                                     error,
                                                                     tmpl_text);
  g_autoptr(mustache_template_t) tmpl = mustache_compile (&_renderer_mustache_data_vfuncs,
                                                          data);

  if (!tmpl)
    return NULL;

  return _renderer_render_mustache_document_internal (tmpl, variables, error);
}

static char *
format_a_href_link (const char *uri,
                    const char *text)
{
  g_autofree char *escaped = g_markup_escape_text (text, -1);

  if (escaped == NULL)
    return NULL;

  return g_strdup_printf ("<a class=\"eos-show-link\" href=\"%s\">%s</a>",
                          uri,
                          escaped);
}

static char *
format_license_link (const char *license)
{
  g_autofree char *escaped = g_uri_escape_string (license, NULL, TRUE);
  g_autofree char *license_link = g_strdup_printf ("license://%s", escaped);
  return format_a_href_link (license_link,
                             eos_get_license_display_name (license));
}

static GVariant *
get_legacy_disclaimer_section_content (const char   *source,
                                       const char   *source_name,
                                       const char   *original_uri,
                                       const char   *license,
                                       const char   *title)
{
  if (g_strcmp0 (source, "wikisource") == 0 ||
      g_strcmp0 (source, "wikibooks") == 0 ||
      g_strcmp0 (source, "wikipedia") == 0)
    {
      g_autofree char *original_link = format_a_href_link (original_uri,
                                                           source_name);
      g_autofree char *license_link = format_license_link (license);
      g_autofree char *disclaimer = g_strdup_printf (_("This page contains content from %s, available under a %s license."),
                                                     original_link,
                                                     license_link);

      return g_variant_new_take_string (g_steal_pointer (&disclaimer));
    }
  else if (g_strcmp0 (source, "wikihow") == 0)
    {
      g_autofree char *wikihow_article_link = format_a_href_link (original_uri,
                                                                  title);
      g_autofree char *wikihow_link = format_a_href_link (_("http://wikihow.com"),
                                                          "WikiHow");
      g_autofree char *disclaimer = g_strdup_printf (_("See %s for more details, videos, pictures and attribution. Courtesy of %s, where anyone can easily learn how to do anything."),
                                                     wikihow_article_link,
                                                     wikihow_link);

      return g_variant_new_take_string (g_steal_pointer (&disclaimer));
    }

  return g_variant_new_boolean (FALSE);
}

static GVariant *
get_legacy_css_files (const char *source)
{
  const char * const empty_css_files[] = { NULL };

  if (g_strcmp0 (source, "wikisource") == 0 ||
      g_strcmp0 (source, "wikibooks") == 0 ||
      g_strcmp0 (source, "wikipedia") == 0)
    {
      const char * const css_files[] = {
        "wikimedia.css",
        NULL
      };
      return g_variant_new_strv (css_files, -1);
    }
  else if (g_strcmp0 (source, "wikihow") == 0)
    {
      const char * const css_files[] = {
        "wikihow.css",
        NULL
      };
      return g_variant_new_strv (css_files, -1);
    }

  return g_variant_new_strv (empty_css_files, -1);
}

static GVariant *
get_legacy_javascript_files (gboolean use_scroll_manager)
{
  g_autoptr(GPtrArray) javascript_files = g_ptr_array_new ();

  g_ptr_array_add (javascript_files, (gpointer) "content-fixes.js");
  g_ptr_array_add (javascript_files, (gpointer) "hide-broken-images.js");

  if (use_scroll_manager)
    g_ptr_array_add (javascript_files, (gpointer) "scroll-manager.js");

  /* NULL-terminate */
  g_ptr_array_add (javascript_files, NULL);

  return g_variant_new_strv ((const char * const *) javascript_files->pdata, -1);
}

static GVariant *
get_legacy_should_include_mathjax (const char *source)
{
  if (g_strcmp0 (source, "wikisource") == 0 ||
      g_strcmp0 (source, "wikibooks") == 0 ||
      g_strcmp0 (source, "wikipedia") == 0)
    return g_variant_new_boolean (TRUE);

  return g_variant_new_boolean (FALSE);
}

static char *
regex_substitute (const char   *regex,
                  const char   *substitution,
                  const char   *content,
                  GError      **error)
{
  g_autoptr(GRegex) regex_compiled = g_regex_new (regex, 0, 0, error);

  if (regex == NULL)
    return NULL;

  return g_regex_replace (regex_compiled,
                          content,
                          -1,
                          0,
                          substitution,
                          0,
                          error);
}

static char *
strip_body_tags (const char  *html,
                 GError     **error)
{
  g_autofree char *stripped_start_tags = regex_substitute ("^\\s*<html>\\s*<body>",
                                                            "",
                                                            html,
                                                            error);

  if (stripped_start_tags == NULL)
    return NULL;

  return regex_substitute ("<\\/body>\\s*<\\/html>\\s*$",
                           "",
                           stripped_start_tags,
                           error);
}

static GFile *
template_file (const char *filename)
{
  g_autofree char *uri = g_strdup_printf ("resource:///com/endlessm/knowledge/data/templates/%s", filename);
  return g_file_new_for_uri (uri);
}

static char *
_renderer_render_legacy_content (EknrRenderer *renderer,
                                 const char   *body_html,
                                 const char   *source,
                                 const char   *source_name,
                                 const char   *original_uri,
                                 const char   *license,
                                 const char   *title,
                                 gboolean      show_title,
                                 gboolean      use_scroll_manager,
                                 GError      **error)
{
  g_autoptr(GFile) file = template_file ("legacy-article.mst");
  g_autofree char *stripped_body = strip_body_tags (body_html, error);
  GVariantDict vardict;
  g_autoptr(GVariant) variant = NULL;
  GVariant *disclaimer = NULL; /* floating */

  if (stripped_body == NULL)
    return NULL;

  disclaimer = get_legacy_disclaimer_section_content (source,
                                                      source_name,
                                                      original_uri,
                                                      license,
                                                      title);

  g_variant_dict_init (&vardict, NULL);

  g_variant_dict_insert_value (&vardict,
                               "title",
                               show_title ? g_variant_new_string (title) : g_variant_new_boolean (FALSE));
  g_variant_dict_insert_value (&vardict, "body-html", g_variant_new_take_string (g_steal_pointer (&stripped_body)));
  g_variant_dict_insert_value (&vardict, "disclaimer", disclaimer);
  g_variant_dict_insert_value (&vardict, "copy-button-text", g_variant_new_string (_("Copy")));
  g_variant_dict_insert_value (&vardict, "css-files", get_legacy_css_files (source));
  g_variant_dict_insert_value (&vardict, "javascript-files", get_legacy_javascript_files (use_scroll_manager));
  g_variant_dict_insert_value (&vardict, "include-mathjax", get_legacy_should_include_mathjax (source));
  g_variant_dict_insert_value (&vardict, "mathjax-path", g_variant_new_string (MATHJAX_PATH));

  variant = g_variant_dict_end (&vardict);

  return eknr_renderer_render_mustache_document_from_file (renderer,
                                                           file,
                                                           variant,
                                                           error);
}

/**
 * eknr_renderer_render_content:
 * @renderer: An #EknrRenderer
 * @body_html: The underlying HTML body
 * @source: Where this content came from
 * @source_name: Name of the source
 * @original_uri: URI this content came from
 * @license: Content license
 * @title: Content title
 * @show_title: %TRUE if the article title should be rendered out too.
 * @use_scroll_manager: %TRUE if the scroll manager should be used, %FALSE otherwise
 * @error: A #GError
 *
 * Render the content and return the rendered content.
 *
 * Returns: (transfer full): A string of rendered HTML or %NULL on error.
 */
char *
eknr_renderer_render_legacy_content (EknrRenderer *renderer,
                                     const char   *body_html,
                                     const char   *source,
                                     const char   *source_name,
                                     const char   *original_uri,
                                     const char   *license,
                                     const char   *title,
                                     gboolean      show_title,
                                     gboolean      use_scroll_manager,
                                     GError       **error)
{
  g_return_val_if_fail (renderer && EKNR_IS_RENDERER (renderer), NULL);

  if (g_strcmp0 (source, "wikipedia") == 0 ||
      g_strcmp0 (source, "wikihow") == 0 ||
      g_strcmp0 (source, "wikisource") == 0 ||
      g_strcmp0 (source, "wikibooks") == 0)
    return _renderer_render_legacy_content (renderer,
                                            body_html,
                                            source,
                                            source_name,
                                            original_uri,
                                            license,
                                            title,
                                            show_title,
                                            use_scroll_manager,
                                            error);

  g_set_error (error,
               EKNR_ERROR,
               EKNR_ERROR_UNKNOWN_LEGACY_SOURCE,
               "Attempted to legacy-render HTML, but no renderer exists for %s",
               source);

  return NULL;
}

static void
eknr_renderer_finalize (GObject *object)
{
  EknrRenderer *self = EKNR_RENDERER (object);
  EknrRendererPrivate *priv = eknr_renderer_get_instance_private (self);

  g_hash_table_unref (priv->cache);

  G_OBJECT_CLASS (eknr_renderer_parent_class)->finalize (object);
}

static void
eknr_renderer_class_init (EknrRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eknr_renderer_finalize;
}

static void
init_i18n (void)
{
  static gsize initialization_value = 0;

  if (g_once_init_enter (&initialization_value))
    {
      bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

      g_once_init_leave (&initialization_value, 1);
    }
}

static void
eknr_renderer_init (EknrRenderer *self)
{
  EknrRendererPrivate *priv = eknr_renderer_get_instance_private (self);

  /* Before initializing the renderer, make sure to call bindtextdomain ()
   * and bind_textdomain_codeset () */
  init_i18n ();

  priv->cache = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       (GDestroyNotify) free_mustache_template);
}

EknrRenderer *
eknr_renderer_new (void)
{
  return EKNR_RENDERER (g_object_new (EKNR_TYPE_RENDERER, NULL));
}
