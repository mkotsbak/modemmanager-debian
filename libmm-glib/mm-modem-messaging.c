/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * libmm -- Access modem status & information from glib applications
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2012 Aleksander Morgado <aleksander@gnu.org>
 * Copyright (C) 2012 Google, Inc.
 */

#include <gio/gio.h>

#include "mm-helpers.h"
#include "mm-common-helpers.h"
#include "mm-errors-types.h"
#include "mm-modem-messaging.h"

/**
 * SECTION: mm-modem-messaging
 * @title: MMModemMessaging
 * @short_description: The Messaging interface
 *
 * The #MMModemMessaging is an object providing access to the methods, signals and
 * properties of the Messaging interface.
 *
 * The Messaging interface is exposed whenever a modem has messaging capabilities.
 */

G_DEFINE_TYPE (MMModemMessaging, mm_modem_messaging, MM_GDBUS_TYPE_MODEM_MESSAGING_PROXY)

struct _MMModemMessagingPrivate {
    /* Supported Storage */
    GMutex supported_storages_mutex;
    guint supported_storages_id;
    GArray *supported_storages;
};

/*****************************************************************************/

/**
 * mm_modem_messaging_get_path:
 * @self: A #MMModemMessaging.
 *
 * Gets the DBus path of the #MMObject which implements this interface.
 *
 * Returns: (transfer none): The DBus path of the #MMObject object.
 */
const gchar *
mm_modem_messaging_get_path (MMModemMessaging *self)
{
    g_return_val_if_fail (MM_IS_MODEM_MESSAGING (self), NULL);

    RETURN_NON_EMPTY_CONSTANT_STRING (
        g_dbus_proxy_get_object_path (G_DBUS_PROXY (self)));
}

/**
 * mm_modem_messaging_dup_path:
 * @self: A #MMModemMessaging.
 *
 * Gets a copy of the DBus path of the #MMObject object which implements this interface.
 *
 * Returns: (transfer full): The DBus path of the #MMObject. The returned value should be freed with g_free().
 */
gchar *
mm_modem_messaging_dup_path (MMModemMessaging *self)
{
    gchar *value;

    g_return_val_if_fail (MM_IS_MODEM_MESSAGING (self), NULL);

    g_object_get (G_OBJECT (self),
                  "g-object-path", &value,
                  NULL);
    RETURN_NON_EMPTY_STRING (value);
}

/*****************************************************************************/

static void
supported_storages_updated (MMModemMessaging *self,
                            GParamSpec *pspec)
{
    g_mutex_lock (&self->priv->supported_storages_mutex);
    {
        GVariant *dictionary;

        if (self->priv->supported_storages)
            g_array_unref (self->priv->supported_storages);

        dictionary = mm_gdbus_modem_messaging_get_supported_storages (MM_GDBUS_MODEM_MESSAGING (self));
        self->priv->supported_storages = (dictionary ?
                                          mm_common_sms_storages_variant_to_garray (dictionary) :
                                          NULL);
    }
    g_mutex_unlock (&self->priv->supported_storages_mutex);
}

static void
ensure_internal_supported_storages (MMModemMessaging *self,
                                    GArray **dup)
{
    g_mutex_lock (&self->priv->supported_storages_mutex);
    {
        /* If this is the first time ever asking for the array, setup the
         * update listener and the initial array, if any. */
        if (!self->priv->supported_storages_id) {
            GVariant *dictionary;

            dictionary = mm_gdbus_modem_messaging_dup_supported_storages (MM_GDBUS_MODEM_MESSAGING (self));
            if (dictionary) {
                self->priv->supported_storages = mm_common_sms_storages_variant_to_garray (dictionary);
                g_variant_unref (dictionary);
            }

            /* No need to clear this signal connection when freeing self */
            self->priv->supported_storages_id =
                g_signal_connect (self,
                                  "notify::supported-storages",
                                  G_CALLBACK (supported_storages_updated),
                                  NULL);
        }

        if (dup && self->priv->supported_storages)
            *dup = g_array_ref (self->priv->supported_storages);
    }
    g_mutex_unlock (&self->priv->supported_storages_mutex);
}

/**
 * mm_modem_messaging_get_supported_storages:
 * @self: A #MMModem.
 * @storages: (out) (array length=n_storages): Return location for the array of #MMSmsStorage values. The returned array should be freed with g_free() when no longer needed.
 * @n_storages: (out): Return location for the number of values in @storages.
 *
 * Gets the list of SMS storages supported by the #MMModem.
 *
 * Returns: %TRUE if @storages and @n_storages are set, %FALSE otherwise.
 */
gboolean
mm_modem_messaging_get_supported_storages (MMModemMessaging *self,
                                           MMSmsStorage **storages,
                                           guint *n_storages)
{
    GArray *array;

    g_return_val_if_fail (MM_IS_MODEM_MESSAGING (self), FALSE);
    g_return_val_if_fail (storages != NULL, FALSE);
    g_return_val_if_fail (n_storages != NULL, FALSE);

    ensure_internal_supported_storages (self, &array);
    if (!array)
        return FALSE;

    *n_storages = array->len;
    *storages = (MMSmsStorage *)g_array_free (array, FALSE);
    return TRUE;
}

/**
 * mm_modem_messaging_peek_supported_storages:
 * @self: A #MMModem.
 * @storages: (out): Return location for the array of #MMSmsStorage values. Do not free the returned array, it is owned by @self.
 * @n_storages: (out): Return location for the number of values in @storages.
 *
 * Gets the list of SMS storages supported by the #MMModem.
 *
 * Returns: %TRUE if @storages and @n_storages are set, %FALSE otherwise.
 */
gboolean
mm_modem_messaging_peek_supported_storages (MMModemMessaging *self,
                                            const MMSmsStorage **storages,
                                            guint *n_storages)
{
    g_return_val_if_fail (MM_IS_MODEM_MESSAGING (self), FALSE);
    g_return_val_if_fail (storages != NULL, FALSE);
    g_return_val_if_fail (n_storages != NULL, FALSE);

    ensure_internal_supported_storages (self, NULL);
    if (!self->priv->supported_storages)
        return FALSE;

    *n_storages = self->priv->supported_storages->len;
    *storages = (MMSmsStorage *)self->priv->supported_storages->data;
    return TRUE;
}

/*****************************************************************************/

/**
 * mm_modem_messaging_get_default_storage:
 * @self: A #MMModem.
 *
 * Gets the default SMS storage used when storing or receiving SMS messages.
 *
 * Returns: the default #MMSmsStorage.
 */
MMSmsStorage
mm_modem_messaging_get_default_storage (MMModemMessaging *self)
{
    g_return_val_if_fail (MM_IS_MODEM_MESSAGING (self), MM_SMS_STORAGE_UNKNOWN);

    return (MMSmsStorage)mm_gdbus_modem_messaging_get_default_storage (MM_GDBUS_MODEM_MESSAGING (self));
}

/*****************************************************************************/

typedef struct {
    MMModemMessaging *self;
    GSimpleAsyncResult *result;
    GCancellable *cancellable;
    gchar **sms_paths;
    GList *sms_objects;
    guint i;
} ListSmsContext;

static void
sms_object_list_free (GList *list)
{
    g_list_free_full (list, (GDestroyNotify) g_object_unref);
}

static void
list_sms_context_complete_and_free (ListSmsContext *ctx)
{
    g_simple_async_result_complete_in_idle (ctx->result);

    g_strfreev (ctx->sms_paths);
    sms_object_list_free (ctx->sms_objects);
    g_object_unref (ctx->result);
    if (ctx->cancellable)
        g_object_unref (ctx->cancellable);
    g_object_ref (ctx->self);
    g_slice_free (ListSmsContext, ctx);
}

/**
 * mm_modem_messaging_list_finish:
 * @self: A #MMModem.
 * @res: The #GAsyncResult obtained from the #GAsyncReadyCallback passed to mm_modem_messaging_list().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with mm_modem_messaging_list().
 *
 * Returns: (element-type ModemManager.Sms) (transfer full): A list of #MMSms objects, or #NULL if either not found or @error is set. The returned value should be freed with g_list_free_full() using g_object_unref() as #GDestroyNotify function.
 */
GList *
mm_modem_messaging_list_finish (MMModemMessaging *self,
                                GAsyncResult *res,
                                GError **error)
{
    GList *list;

    g_return_val_if_fail (MM_IS_MODEM_MESSAGING (self), FALSE);

    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    list = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res));

    /* The list we got, including the objects within, is owned by the async result;
     * so we'll make sure we return a new list */
    g_list_foreach (list, (GFunc)g_object_ref, NULL);
    return g_list_copy (list);
}

static void create_next_sms (ListSmsContext *ctx);

static void
list_build_object_ready (GDBusConnection *connection,
                         GAsyncResult *res,
                         ListSmsContext *ctx)
{
    GError *error = NULL;
    GObject *sms;
    GObject *source_object;

    source_object = g_async_result_get_source_object (res);
    sms = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object), res, &error);
    g_object_unref (source_object);

    if (error) {
        g_simple_async_result_take_error (ctx->result, error);
        list_sms_context_complete_and_free (ctx);
        return;
    }

    /* Keep the object */
    ctx->sms_objects = g_list_prepend (ctx->sms_objects, sms);

    /* If no more smss, just end here. */
    if (!ctx->sms_paths[++ctx->i]) {
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   ctx->sms_objects,
                                                   (GDestroyNotify)sms_object_list_free);
        ctx->sms_objects = NULL;
        list_sms_context_complete_and_free (ctx);
        return;
    }

    /* Keep on creating next object */
    create_next_sms (ctx);
}

static void
create_next_sms (ListSmsContext *ctx)
{
    g_async_initable_new_async (MM_TYPE_SMS,
                                G_PRIORITY_DEFAULT,
                                ctx->cancellable,
                                (GAsyncReadyCallback)list_build_object_ready,
                                ctx,
                                "g-flags",          G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                "g-name",           MM_DBUS_SERVICE,
                                "g-connection",     g_dbus_proxy_get_connection (G_DBUS_PROXY (ctx->self)),
                                "g-object-path",    ctx->sms_paths[ctx->i],
                                "g-interface-name", "org.freedesktop.ModemManager1.Sms",
                                NULL);
}

/**
 * mm_modem_messaging_list:
 * @self: A #MMModemMessaging.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied or %NULL.
 * @user_data: User data to pass to @callback.
 *
 * Asynchronously lists the #MMSms objects in the modem.
 *
 * When the operation is finished, @callback will be invoked in the <link linkend="g-main-context-push-thread-default">thread-default main loop</link> of the thread you are calling this method from.
 * You can then call mm_modem_messaging_list_finish() to get the result of the operation.
 *
 * See mm_modem_messaging_list_sync() for the synchronous, blocking version of this method.
 */
void
mm_modem_messaging_list (MMModemMessaging *self,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
    ListSmsContext *ctx;

    g_return_if_fail (MM_IS_MODEM_MESSAGING (self));

    ctx = g_slice_new0 (ListSmsContext);
    ctx->self = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             mm_modem_messaging_list);
    if (cancellable)
        ctx->cancellable = g_object_ref (cancellable);

    ctx->sms_paths = mm_gdbus_modem_messaging_dup_messages (MM_GDBUS_MODEM_MESSAGING (self));

    /* If no SMS, just end here. */
    if (!ctx->sms_paths || !ctx->sms_paths[0]) {
        g_simple_async_result_set_op_res_gpointer (ctx->result, NULL, NULL);
        list_sms_context_complete_and_free (ctx);
        return;
    }

    /* Got list of paths. If at least one found, start creating objects for each */
    ctx->i = 0;
    create_next_sms (ctx);
}

/**
 * mm_modem_messaging_list_sync:
 * @self: A #MMModemMessaging.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Synchronously lists the #MMSms objects in the modem.
 *
 * The calling thread is blocked until a reply is received. See mm_modem_messaging_list()
 * for the asynchronous version of this method.
 *
 * Returns: (element-type MMSms) (transfer full): A list of #MMSms objects, or #NULL if either not found or @error is set. The returned value should be freed with g_list_free_full() using g_object_unref() as #GDestroyNotify function.
 */
GList *
mm_modem_messaging_list_sync (MMModemMessaging *self,
                              GCancellable *cancellable,
                              GError **error)
{
    GList *sms_objects = NULL;
    gchar **sms_paths = NULL;
    guint i;

    g_return_val_if_fail (MM_IS_MODEM_MESSAGING (self), NULL);

    sms_paths = mm_gdbus_modem_messaging_dup_messages (MM_GDBUS_MODEM_MESSAGING (self));

    /* Only non-empty lists are returned */
    if (!sms_paths)
        return NULL;

    for (i = 0; sms_paths[i]; i++) {
        GObject *sms;

        sms = g_initable_new (MM_TYPE_SMS,
                                 cancellable,
                                 error,
                                 "g-flags",          G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                 "g-name",           MM_DBUS_SERVICE,
                                 "g-connection",     g_dbus_proxy_get_connection (G_DBUS_PROXY (self)),
                                 "g-object-path",    sms_paths[i],
                                 "g-interface-name", "org.freedesktop.ModemManager1.Sms",
                              NULL);
        if (!sms) {
            sms_object_list_free (sms_objects);
            g_strfreev (sms_paths);
            return NULL;
        }

        /* Keep the object */
        sms_objects = g_list_prepend (sms_objects, sms);
    }

    g_strfreev (sms_paths);
    return sms_objects;
}

/*****************************************************************************/

typedef struct {
    GSimpleAsyncResult *result;
    GCancellable *cancellable;
} CreateSmsContext;

static void
create_sms_context_complete_and_free (CreateSmsContext *ctx)
{
    g_simple_async_result_complete (ctx->result);
    g_object_unref (ctx->result);
    if (ctx->cancellable)
        g_object_unref (ctx->cancellable);
    g_slice_free (CreateSmsContext, ctx);
}

/**
 * mm_modem_messaging_create_finish:
 * @self: A #MMModemMessaging.
 * @res: The #GAsyncResult obtained from the #GAsyncReadyCallback passed to mm_modem_messaging_create().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with mm_modem_messaging_create().
 *
 * Returns: (transfer full): A newly created #MMSms, or %NULL if @error is set. The returned value should be freed with g_object_unref().
 */
MMSms *
mm_modem_messaging_create_finish (MMModemMessaging *self,
                                  GAsyncResult *res,
                                  GError **error)
{
    g_return_val_if_fail (MM_IS_MODEM_MESSAGING (self), NULL);

    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    return g_object_ref (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res)));
}

static void
new_sms_object_ready (GDBusConnection *connection,
                      GAsyncResult *res,
                      CreateSmsContext *ctx)
{
    GError *error = NULL;
    GObject *sms;
    GObject *source_object;

    source_object = g_async_result_get_source_object (res);
    sms = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object), res, &error);
    g_object_unref (source_object);

    if (error)
        g_simple_async_result_take_error (ctx->result, error);
    else
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   sms,
                                                   (GDestroyNotify)g_object_unref);

    create_sms_context_complete_and_free (ctx);
}

static void
create_sms_ready (MMModemMessaging *self,
                  GAsyncResult *res,
                  CreateSmsContext *ctx)
{
    GError *error = NULL;
    gchar *sms_path = NULL;

    if (!mm_gdbus_modem_messaging_call_create_finish (MM_GDBUS_MODEM_MESSAGING (self),
                                                      &sms_path,
                                                      res,
                                                      &error)) {
        g_simple_async_result_take_error (ctx->result, error);
        create_sms_context_complete_and_free (ctx);
        g_free (sms_path);
        return;
    }

    g_async_initable_new_async (MM_TYPE_SMS,
                                G_PRIORITY_DEFAULT,
                                ctx->cancellable,
                                (GAsyncReadyCallback)new_sms_object_ready,
                                ctx,
                                "g-flags",          G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                "g-name",           MM_DBUS_SERVICE,
                                "g-connection",     g_dbus_proxy_get_connection (G_DBUS_PROXY (self)),
                                "g-object-path",    sms_path,
                                "g-interface-name", "org.freedesktop.ModemManager1.Sms",
                                NULL);
    g_free (sms_path);
}

/**
 * mm_modem_messaging_create_sms:
 * @self: A #MMModemMessaging.
 * @properties: A ##MMSmsProperties object with the properties to use.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied or %NULL.
 * @user_data: User data to pass to @callback.
 *
 * Asynchronously creates a new #MMSms in the modem.
 *
 * When the operation is finished, @callback will be invoked in the <link linkend="g-main-context-push-thread-default">thread-default main loop</link> of the thread you are calling this method from.
 * You can then call mm_modem_messaging_create_finish() to get the result of the operation.
 *
 * See mm_modem_messaging_create_sync() for the synchronous, blocking version of this method.
 */
void
mm_modem_messaging_create (MMModemMessaging *self,
                           MMSmsProperties *properties,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
    CreateSmsContext *ctx;
    GVariant *dictionary;

    g_return_if_fail (MM_IS_MODEM_MESSAGING (self));

    ctx = g_slice_new0 (CreateSmsContext);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             mm_modem_messaging_create);
    if (cancellable)
        ctx->cancellable = g_object_ref (cancellable);

    dictionary = (mm_sms_properties_get_dictionary (properties));
    mm_gdbus_modem_messaging_call_create (
        MM_GDBUS_MODEM_MESSAGING (self),
        dictionary,
        cancellable,
        (GAsyncReadyCallback)create_sms_ready,
        ctx);

    g_variant_unref (dictionary);
}

/**
 * mm_modem_messaging_create_sync:
 * @self: A #MMModemMessaging.
 * @properties: A ##MMSmsProperties object with the properties to use.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Synchronously creates a new #MMSms in the modem.
 *
 * The calling thread is blocked until a reply is received. See mm_modem_messaging_create()
 * for the asynchronous version of this method.
 *
 * Returns: (transfer full): A newly created #MMSms, or %NULL if @error is set. The returned value should be freed with g_object_unref().
 */
MMSms *
mm_modem_messaging_create_sync (MMModemMessaging *self,
                                MMSmsProperties *properties,
                                GCancellable *cancellable,
                                GError **error)
{
    MMSms *sms = NULL;
    gchar *sms_path = NULL;
    GVariant *dictionary;

    g_return_val_if_fail (MM_IS_MODEM_MESSAGING (self), NULL);

    dictionary = (mm_sms_properties_get_dictionary (properties));
    mm_gdbus_modem_messaging_call_create_sync (MM_GDBUS_MODEM_MESSAGING (self),
                                               dictionary,
                                               &sms_path,
                                               cancellable,
                                               error);
    if (sms_path) {
        sms = g_initable_new (MM_TYPE_SMS,
                              cancellable,
                              error,
                              "g-flags",          G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                              "g-name",           MM_DBUS_SERVICE,
                              "g-connection",     g_dbus_proxy_get_connection (G_DBUS_PROXY (self)),
                              "g-object-path",    sms_path,
                              "g-interface-name", "org.freedesktop.ModemManager1.Sms",
                              NULL);
        g_free (sms_path);
    }

    g_variant_unref (dictionary);

    return (sms ? MM_SMS (sms) : NULL);
}

/*****************************************************************************/

/**
 * mm_modem_messaging_delete_finish:
 * @self: A #MMModemMessaging.
 * @res: The #GAsyncResult obtained from the #GAsyncReadyCallback passed to mm_modem_messaging_delete().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with mm_modem_messaging_delete().
 *
 * Returns: %TRUE if the sms was deleted, %FALSE if @error is set.
 */
gboolean
mm_modem_messaging_delete_finish (MMModemMessaging *self,
                                  GAsyncResult *res,
                                  GError **error)
{
    g_return_val_if_fail (MM_IS_MODEM_MESSAGING (self), FALSE);

    return mm_gdbus_modem_messaging_call_delete_finish (MM_GDBUS_MODEM_MESSAGING (self), res, error);
}

/**
 * mm_modem_messaging_delete:
 * @self: A #MMModemMessaging.
 * @sms: Path of the #MMSms to delete.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied or %NULL.
 * @user_data: User data to pass to @callback.
 *
 * Asynchronously deletes a given #MMSms from the modem.
 *
 * When the operation is finished, @callback will be invoked in the <link linkend="g-main-context-push-thread-default">thread-default main loop</link> of the thread you are calling this method from.
 * You can then call mm_modem_messaging_delete_finish() to get the result of the operation.
 *
 * See mm_modem_messaging_delete_sync() for the synchronous, blocking version of this method.
 */
void
mm_modem_messaging_delete (MMModemMessaging *self,
                           const gchar *sms,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
    g_return_if_fail (MM_IS_MODEM_MESSAGING (self));

    mm_gdbus_modem_messaging_call_delete (MM_GDBUS_MODEM_MESSAGING (self),
                                          sms,
                                          cancellable,
                                          callback,
                                          user_data);
}

/**
 * mm_modem_messaging_delete_sync:
 * @self: A #MMModemMessaging.
 * @sms: Path of the #MMSms to delete.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.

 * Synchronously deletes a given #MMSms from the modem.
 *
 * The calling thread is blocked until a reply is received. See mm_modem_messaging_delete()
 * for the asynchronous version of this method.
 *
 * Returns: %TRUE if the SMS was deleted, %FALSE if @error is set.
 */
gboolean
mm_modem_messaging_delete_sync (MMModemMessaging *self,
                                const gchar *sms,
                                GCancellable *cancellable,
                                GError **error)
{
    g_return_val_if_fail (MM_IS_MODEM_MESSAGING (self), FALSE);

    return mm_gdbus_modem_messaging_call_delete_sync (MM_GDBUS_MODEM_MESSAGING (self),
                                                      sms,
                                                      cancellable,
                                                      error);
}

/*****************************************************************************/

static void
mm_modem_messaging_init (MMModemMessaging *self)
{
    /* Setup private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                              MM_TYPE_MODEM_MESSAGING,
                                              MMModemMessagingPrivate);
    g_mutex_init (&self->priv->supported_storages_mutex);
}

static void
finalize (GObject *object)
{
    MMModemMessaging *self = MM_MODEM_MESSAGING (object);

    g_mutex_clear (&self->priv->supported_storages_mutex);

    if (self->priv->supported_storages)
        g_array_unref (self->priv->supported_storages);

    G_OBJECT_CLASS (mm_modem_messaging_parent_class)->finalize (object);
}

static void
mm_modem_messaging_class_init (MMModemMessagingClass *modem_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (modem_class);

    g_type_class_add_private (object_class, sizeof (MMModemMessagingPrivate));

    /* Virtual methods */
    object_class->finalize = finalize;
}