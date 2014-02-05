/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 * Copyright (C) 2012 Google, Inc.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <ModemManager.h>
#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>
#include "mm-log.h"
#include "mm-modem-helpers.h"
#include "mm-base-modem-at.h"

#include "mm-sim-novatel-lte.h"

G_DEFINE_TYPE (MMSimNovatelLte, mm_sim_novatel_lte, MM_TYPE_SIM);

/*****************************************************************************/
/* IMSI loading */

static gchar *
load_imsi_finish (MMSim *self,
                  GAsyncResult *res,
                  GError **error)
{
    gchar *imsi;

    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    imsi = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res));
    mm_dbg ("loaded IMSI: %s", imsi);
    return g_strdup (imsi);
}

static void
imsi_read_ready (MMBaseModem *modem,
                 GAsyncResult *res,
                 GSimpleAsyncResult *simple)
{
    GError *error = NULL;
    const gchar *response, *str;
    gchar buf[19];
    gchar imsi[16];
    gsize len = 0;
    gint sw1, sw2;
    gint i;

    response = mm_base_modem_at_command_finish (modem, res, &error);
    if (!response) {
        g_simple_async_result_take_error (simple, error);
        g_simple_async_result_complete (simple);
        g_object_unref (simple);
        return;
    }

    memset (buf, 0, sizeof (buf));
    str = mm_strip_tag (response, "+CRSM:");

    /* With or without quotes... */
    if (sscanf (str, "%d,%d,\"%18c\"", &sw1, &sw2, (char *) &buf) != 3 &&
        sscanf (str, "%d,%d,%18c", &sw1, &sw2, (char *) &buf) != 3) {
        g_simple_async_result_set_error (simple,
                                         MM_CORE_ERROR,
                                         MM_CORE_ERROR_FAILED,
                                         "Failed to parse the CRSM response: '%s'",
                                         response);
        g_simple_async_result_complete (simple);
        g_object_unref (simple);
        return;
    }

    if ((sw1 != 0x90 || sw2 != 0x00) &&
        (sw1 != 0x91) &&
        (sw1 != 0x92) &&
        (sw1 != 0x9f)) {
        g_simple_async_result_set_error (simple,
                                         MM_CORE_ERROR,
                                         MM_CORE_ERROR_FAILED,
                                         "SIM failed to handle CRSM request (sw1 %d sw2 %d)",
                                         sw1, sw2);
        g_simple_async_result_complete (simple);
        g_object_unref (simple);
        return;
    }

    /* Make sure the buffer is only digits or 'F' */
    for (len = 0; len < sizeof (buf) && buf[len]; len++) {
        if (isdigit (buf[len]))
            continue;
        if (buf[len] == 'F' || buf[len] == 'f') {
            buf[len] = 'F';  /* canonicalize the F */
            continue;
        }
        if (buf[len] == '\"') {
            buf[len] = 0;
            break;
        }

        /* Invalid character */
        g_simple_async_result_set_error (simple,
                                         MM_CORE_ERROR,
                                         MM_CORE_ERROR_FAILED,
                                         "CRSM IMSI response contained invalid character '%c'",
                                         buf[len]);
        g_simple_async_result_complete (simple);
        g_object_unref (simple);
        return;
    }

    /* BCD encoded IMSIs plus the length byte and parity are 18 digits long */
    if (len != 18) {
        g_simple_async_result_set_error (simple,
                                         MM_CORE_ERROR,
                                         MM_CORE_ERROR_FAILED,
                                         "Invalid +CRSM IMSI response size (was %zd, expected 18)",
                                         len);
        g_simple_async_result_complete (simple);
        g_object_unref (simple);
        return;
    }

    /* Skip the length byte (digit 0-1) and parity (digit 3). Swap digits in
     * the EFimsi response to get the actual IMSI, each group of 2 digits is
     * reversed in the +CRSM response.  i.e.:
     *
     *    **0*21436587a9cbed -> 0123456789abcde
     */
    memset (imsi, 0, sizeof (imsi));
    imsi[0] = buf[2];
    for (i = 1; i < 8; i++) {
        imsi[(i * 2) - 1] = buf[(i * 2) + 3];
        imsi[i * 2] = buf[(i * 2) + 2];
    }

    /* Zero out the first F, if any, for IMSIs shorter than 15 digits */
    for (i = 0; i < 15; i++) {
        if (imsi[i] == 'F') {
            imsi[i++] = 0;
            break;
        }
    }

    /* Ensure all 'F's, if any, are at the end */
    for (; i < 15; i++) {
        if (imsi[i] != 'F') {
            g_simple_async_result_set_error (simple,
                                             MM_CORE_ERROR,
                                             MM_CORE_ERROR_FAILED,
                                             "Invalid +CRSM IMSI length (unexpected F)");
            g_simple_async_result_complete (simple);
            g_object_unref (simple);
            return;
        }
    }

    g_simple_async_result_set_op_res_gpointer (simple, imsi, NULL);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
}

static void
load_imsi (MMSim *self,
           GAsyncReadyCallback callback,
           gpointer user_data)
{
    MMBaseModem *modem = NULL;

    g_object_get (self,
                  MM_SIM_MODEM, &modem,
                  NULL);

    mm_dbg ("loading (Novatel LTE) IMSI...");
    mm_base_modem_at_command (
        MM_BASE_MODEM (modem),
        "+CRSM=176,28423,0,0,9",
        3,
        FALSE,
        (GAsyncReadyCallback)imsi_read_ready,
        g_simple_async_result_new (G_OBJECT (self),
                                   callback,
                                   user_data,
                                   load_imsi));
    g_object_unref (modem);
}

/*****************************************************************************/

MMSim *
mm_sim_novatel_lte_new_finish (GAsyncResult *res,
                               GError **error)
{
    GObject *source;
    GObject *sim;

    source = g_async_result_get_source_object (res);
    sim = g_async_initable_new_finish (G_ASYNC_INITABLE (source), res, error);
    g_object_unref (source);

    if (!sim)
        return NULL;

    /* Only export valid SIMs */
    mm_sim_export (MM_SIM (sim));

    return MM_SIM (sim);
}

void
mm_sim_novatel_lte_new (MMBaseModem *modem,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
    g_async_initable_new_async (MM_TYPE_SIM_NOVATEL_LTE,
                                G_PRIORITY_DEFAULT,
                                cancellable,
                                callback,
                                user_data,
                                MM_SIM_MODEM, modem,
                                NULL);
}

static void
mm_sim_novatel_lte_init (MMSimNovatelLte *self)
{
}

static void
mm_sim_novatel_lte_class_init (MMSimNovatelLteClass *klass)
{
    MMSimClass *sim_class = MM_SIM_CLASS (klass);

    sim_class->load_imsi = load_imsi;
    sim_class->load_imsi_finish = load_imsi_finish;
}
