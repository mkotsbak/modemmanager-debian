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
 * Copyright (C) 2013 Aleksander Morgado <aleksander@gnu.org>
 */

#ifndef MM_PORT_MBIM_H
#define MM_PORT_MBIM_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libmbim-glib.h>

#include "mm-port.h"

#define MM_TYPE_PORT_MBIM            (mm_port_mbim_get_type ())
#define MM_PORT_MBIM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_PORT_MBIM, MMPortMbim))
#define MM_PORT_MBIM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MM_TYPE_PORT_MBIM, MMPortMbimClass))
#define MM_IS_PORT_MBIM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_PORT_MBIM))
#define MM_IS_PORT_MBIM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MM_TYPE_PORT_MBIM))
#define MM_PORT_MBIM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MM_TYPE_PORT_MBIM, MMPortMbimClass))

typedef struct _MMPortMbim MMPortMbim;
typedef struct _MMPortMbimClass MMPortMbimClass;
typedef struct _MMPortMbimPrivate MMPortMbimPrivate;

struct _MMPortMbim {
    MMPort parent;
    MMPortMbimPrivate *priv;
};

struct _MMPortMbimClass {
    MMPortClass parent;
};

GType mm_port_mbim_get_type (void);

MMPortMbim *mm_port_mbim_new (const gchar *name);

void     mm_port_mbim_open         (MMPortMbim *self,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data);
gboolean mm_port_mbim_open_finish  (MMPortMbim *self,
                                    GAsyncResult *res,
                                    GError **error);
gboolean mm_port_mbim_is_open      (MMPortMbim *self);
void     mm_port_mbim_close        (MMPortMbim *self,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data);
gboolean mm_port_mbim_close_finish (MMPortMbim *self,
                                    GAsyncResult *res,
                                    GError **error);

MbimDevice *mm_port_mbim_peek_device (MMPortMbim *self);

#endif /* MM_PORT_MBIM_H */