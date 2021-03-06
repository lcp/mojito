/*
 * Mojito - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <config.h>

#include <glib.h>
#include "mojito-online.h"
#include "mojito-debug.h"
#include "mojito-marshals.h"

/* This is the common infrastructure */
typedef struct {
  MojitoOnlineNotify callback;
  gpointer user_data;
} ListenerData;

static GList *listeners = NULL;

static gboolean online_init (void);

void
mojito_online_add_notify (MojitoOnlineNotify callback, gpointer user_data)
{
  ListenerData *data;

  if (!online_init ())
    return;

  data = g_slice_new (ListenerData);
  data->callback = callback;
  data->user_data = user_data;

  listeners = g_list_prepend (listeners, data);
}

void
mojito_online_remove_notify (MojitoOnlineNotify callback, gpointer user_data)
{
  GList *l = listeners;

  while (l) {
    ListenerData *data = l->data;
    if (data->callback == callback && data->user_data == user_data) {
      GList *next = l->next;
      listeners = g_list_delete_link (listeners, l);
      l = next;
    } else {
      l = l->next;
    }
  }
}

static void
emit_notify (gboolean online)
{
  GList *l;

  MOJITO_DEBUG (ONLINE, "Now %s", online ? "online" : "offline");

  for (l = listeners; l; l = l->next) {
    ListenerData *data = l->data;
    data->callback (online, data->user_data);
  }
}

#if WITH_ONLINE_ALWAYS

/*
 * A bit nasty but this case never uses emit_notify we get a compile warning
 * otherwise.
 */
static const gpointer dummy = &emit_notify;

static gboolean
online_init (void)
{
  return FALSE;
}

gboolean
mojito_is_online (void)
{
  return TRUE;
}

#endif

#if WITH_ONLINE_NM
#include <libnm-glib/nm-client.h>

/*
 * Use NMClient since it correctly handles the NetworkManager service
 * appearing and disappearing, as can happen at boot time, or during
 * a network subsystem restart.
 */
static NMClient *client = NULL;

static gboolean
we_are_online (gpointer user_data)
{
  emit_notify (mojito_is_online ());
  return FALSE;
}

static void
state_changed (NMClient        *client,
	       const GParamSpec *pspec,
	       gpointer          data)
{
  if (mojito_is_online()) {
    /* NM is notifying us too early - workaround that */
    g_timeout_add (1500, (GSourceFunc)we_are_online, NULL);
  } else {
    emit_notify (FALSE); /* mojito_is_online ()); */
  }
}

static gboolean
online_init (void)
{
  if (!client) {
    client = nm_client_new();
    g_signal_connect (client, "notify::" NM_CLIENT_STATE,
		      G_CALLBACK (state_changed), NULL);
  }
  return TRUE;
}

gboolean
mojito_is_online (void)
{
  NMState state = NM_STATE_UNKNOWN;

  if (!online_init ())
    return TRUE;

  g_object_get (G_OBJECT (client), NM_CLIENT_STATE, &state, NULL);

  switch (state) {
  case NM_STATE_CONNECTED:
    return TRUE;
  case NM_STATE_CONNECTING:
  case NM_STATE_ASLEEP:
  case NM_STATE_DISCONNECTED:
  case NM_STATE_UNKNOWN:
  default:
    return FALSE;
  }
}

#endif


#if WITH_ONLINE_CONNMAN
#include <string.h>
#include <dbus/dbus-glib.h>

static DBusGProxy *proxy = NULL;
static gboolean current_state;

#define STRING_VARIANT_HASHTABLE (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))

static void
prop_changed (DBusGProxy *proxy, const char *key, GValue *v, gpointer user_data)
{
  const char *s;

  if (strcmp (key, "State") != 0)
    return;

  s = g_value_get_string (v);

  current_state = (g_strcmp0 (s, "online") == 0);

  emit_notify (current_state);
}

static void
got_props_cb (DBusGProxy *proxy, DBusGProxyCall *call, void *user_data)
{
  GHashTable *hash;
  GError *error = NULL;
  const GValue *value;
  const char *s;

  if (!dbus_g_proxy_end_call (proxy, call, &error, STRING_VARIANT_HASHTABLE, &hash, G_TYPE_INVALID)) {
    g_printerr ("Cannot get current online state: %s", error->message);
    g_error_free (error);
    return;
  }

  value = g_hash_table_lookup (hash, "State");
  if (value) {
    s = g_value_get_string (value);
    current_state = (g_strcmp0 (s, "online") == 0);
    emit_notify (current_state);
  }

  g_hash_table_unref (hash);
}

static gboolean
online_init (void)
{
  DBusGConnection *conn;

  if (proxy)
    return TRUE;

  conn = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
  if (!conn) {
    g_warning ("Cannot get connection to system message bus");
    return FALSE;
  }

  proxy = dbus_g_proxy_new_for_name (conn, "org.moblin.connman",
                                     "/", "org.moblin.connman.Manager");

  dbus_g_object_register_marshaller (mojito_marshal_VOID__STRING_BOXED,
                                     G_TYPE_NONE,
                                     G_TYPE_STRING,
                                     G_TYPE_BOXED,
                                     G_TYPE_INVALID);
  dbus_g_proxy_add_signal (proxy, "PropertyChanged",
                           G_TYPE_STRING, G_TYPE_VALUE, NULL);
  dbus_g_proxy_connect_signal (proxy, "PropertyChanged",
                               (GCallback)prop_changed, NULL, NULL);

  current_state = FALSE;

  /* Get the current state */
  dbus_g_proxy_begin_call (proxy, "GetProperties",
                           got_props_cb, NULL, NULL,
                           G_TYPE_INVALID);

  return TRUE;
}


gboolean
mojito_is_online (void)
{
  if (!online_init ())
    return TRUE;

  return current_state;
}
#endif

#if WITH_ONLINE_TEST
#include "mojito-online-testui.c"
#endif
