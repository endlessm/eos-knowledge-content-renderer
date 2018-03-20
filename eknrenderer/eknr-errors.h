/* Copyright 2018 Endless Mobile, Inc. */

#pragma once

#if !(defined(_EKN_RENDERER_INSIDE_EKNR_H) || defined(COMPILING_EKNR))
#error "Please do not include this header file directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * EKNR_ERROR:
 *
 * Error doamin for Eknr.
 */
#define EKNR_ERROR (eknr_error ())

GQuark eknr_error (void);

/**
 * EknrError:
 * @EKNR_ERROR_SUBSTITUTION_FAILED: Template substitution failed
 * @EKNR_ERROR_UNKNOWN_LEGACY_SOURCE: Don't know how to deal with the specified source type
 *
 * Error codes for the %EKNR_ERROR error domain
 */
typedef enum {
  EKNR_ERROR_SUBSTITUTION_FAILED,
  EKNR_ERROR_UNKNOWN_LEGACY_SOURCE
} EknrError;

G_END_DECLS
