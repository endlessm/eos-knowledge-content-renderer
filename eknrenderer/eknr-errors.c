/* Copyright 2018 Endless Mobile, Inc. */

#include <string.h>
#include <stdlib.h>

#include "eknr-errors.h"

GQuark
eknr_error (void)
{
  return g_quark_from_static_string ("eknr-error");
}

G_DEFINE_QUARK (eknr-error, eknr_error)
