/*
 * Mojito Facebook service support
 *
 * Copyright (C) 2009 Novell, Inc.
 *
 * Author: Gary Ching-Pang Lin <glin@novell.com>
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
#include <time.h>
#include <string.h>
#include "facebook.h"
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-utils.h>
#include <mojito/mojito-web.h>
#include <mojito-keyfob/mojito-keyfob.h>
#include <mojito-keystore/mojito-keystore.h>
#include <rest/oauth-proxy.h>
#include <rest/rest-xml-parser.h>
#include <mojito/mojito-online.h>

G_DEFINE_TYPE (MojitoServiceFacebook, mojito_service_facebook, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_FACEBOOK, MojitoServiceFacebookPrivate))

struct _MojitoServiceFacebookPrivate {
  gboolean running;
  RestProxy *proxy;
  char *uid;
  char *display_name;
  char *profile_url;
  char *pic_square;
};

static GList *service_list;

RestXmlNode *
node_from_call (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *root;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_message ("Error from Facebook: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  if (root == NULL) {
    g_message ("Error from Facebook: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  if (strcmp (root->name, "error_response") == 0) {
    RestXmlNode *node;
    node = rest_xml_node_find (root, "error_msg");
    g_message ("Error response from Facebook: %s\n", node->content);
    rest_xml_node_unref (root);
    return NULL;
  } else {
    return root;
  }
}

/*
 * For a given parent @node, get the child node called @name and return a copy
 * of the content, or NULL. If the content is the empty string, NULL is
 * returned.
 */
static char *
get_child_node_value (RestXmlNode *node, const char *name)
{
  RestXmlNode *subnode;

  if (!node || !name)
    return NULL;

  subnode = rest_xml_node_find (node, name);
  if (!subnode)
    return NULL;

  if (subnode->content && subnode->content[0])
    return g_strdup (subnode->content);
  else
    return NULL;
}

static char *
get_utc_date (const char *s)
{
  struct tm tm;
  time_t t;

  if (s == NULL)
    return NULL;

  strptime (s, "%s", &tm);
  t = mktime (&tm);

  return mojito_time_t_to_string (t);
}

static void
got_status_cb (RestProxyCall *call,
             GError        *error,
             GObject       *weak_object,
             gpointer       userdata)
{
  MojitoService *service = MOJITO_SERVICE (weak_object);
  MojitoServiceFacebookPrivate *priv = MOJITO_SERVICE_FACEBOOK (service)->priv;
  RestXmlNode *root, *node;
  MojitoSet *set;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  root = node_from_call (call);
  if (!root)
    return;

  set = mojito_item_set_new ();
  /*
  example return XML:
  fields: uid, name, pic_square, status
  <user>
    <name>Geckosuse Novell</name>
    <status>
      <message>Test status on moblin.</message>
      <time>1249546470</time>
      <status_id>137108505991</status_id>
    </status>
    <uid>100000151172819</uid>
    <pic_square>https://secure-profile.facebook.com/v230/109/50/q100000151172819_377.jpg</pic_square>
    <profile_url>http://www.facebook.com/profile.php?id=100000151172819</profile_url>
  </user>
  */

  node = rest_xml_node_find (root, "user");

  while (node) {
    MojitoItem *item;
    char *id, *uid, *time, *message, *name, *pic_square, *profile_url;
    RestXmlNode *status_node;

    item = mojito_item_new ();
    mojito_item_set_service (item, service);

    uid = get_child_node_value (node, "uid");
    status_node = rest_xml_node_find (node, "status");
    if(!uid || !status_node){
      node = node->next;
      continue;
    }

    time = get_child_node_value (status_node, "time");
    if(!time){
      node = node->next;
      continue;
    }

    /* Construct item ID */
    id = g_strconcat ("facebook-", uid, "-", time, NULL);
    mojito_item_take (item, "id", id);

    mojito_item_take (item, "date", get_utc_date (time));

    /* Get the status message */
    message = get_child_node_value (status_node, "message");
    if (!message) {
      node = node->next;
      continue;
    }
    mojito_item_put (item, "content", message);
    mojito_item_put (item, "authorid", uid);
    name = get_child_node_value (node, "name");
    if (name)
      mojito_item_put (item, "author", name);
    else
      mojito_item_put (item, "author", priv->display_name);

    pic_square = get_child_node_value (node, "pic_square");
    if (pic_square)
      mojito_item_request_image_fetch (item, "authoricon", pic_square);

    profile_url = get_child_node_value (node, "profile_url");
    if (profile_url)
      mojito_item_put (item, "url", profile_url);
    else
      mojito_item_put (item, "url", priv->profile_url);

    mojito_set_add (set, G_OBJECT (item));
    g_object_unref (item);

    node = node->next;
  }

  rest_xml_node_unref (root);

  if (!mojito_set_is_empty (set))
    mojito_service_emit_refreshed (service, set);

  mojito_set_unref (set);
}

static void
get_status_updates (MojitoServiceFacebook *service)
{
  MojitoServiceFacebookPrivate *priv = service->priv;
  RestProxyCall *call;
  GHashTable *params = NULL;
  char *fql;

  if (!priv->uid)
    return;

  call = rest_proxy_new_call (priv->proxy);

  g_object_get (service, "params", &params, NULL);

  if (params && g_hash_table_lookup (params, "own")) {
    rest_proxy_call_set_function (call, "Users.getInfo");
    rest_proxy_call_add_params (call,
                                "uids", priv->uid,
                                "fields", "name, profile_url, pic_square, status",
                                NULL);
  } else {
    /* Get the friend's name, status, profile_url, pic_square*/
    rest_proxy_call_set_function (call, "fql.query");
    fql = g_strdup_printf ("SELECT uid,name,status,profile_url,pic_square "
                           "FROM user WHERE uid IN "
                           "(SELECT uid2 FROM friend WHERE uid1= %s)", priv->uid);
    rest_proxy_call_add_param (call, "query", fql);
  }
  if (params)
    g_hash_table_unref (params);

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback)got_status_cb,
                         (GObject*)service,
                         NULL,
                         NULL);
}

static const char **
get_static_caps (MojitoService *service)
{
  static const char * caps[] = {
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (MojitoService *service)
{
  MojitoServiceFacebook *facebook = MOJITO_SERVICE_FACEBOOK (service);
  static const char * caps[] = {
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };
  static const char * no_caps[] = { NULL };

  if (facebook->priv->uid)
    return caps;
  else
    return no_caps;
}

static void
got_user_info_cb (RestProxyCall *call,
                  const GError  *error,
                  GObject       *weak_object,
                  gpointer       userdata)
{
  MojitoService *service = MOJITO_SERVICE (weak_object);
  MojitoServiceFacebook *facebook = MOJITO_SERVICE_FACEBOOK (service);
  MojitoServiceFacebookPrivate *priv = facebook->priv;
  RestXmlNode *node;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  node = node_from_call (call);
  if (!node)
    return;

  priv->display_name = get_child_node_value(node, "name");
  priv->profile_url = get_child_node_value(node, "profile_url");
  priv->pic_square = get_child_node_value(node, "pic_square");
  rest_xml_node_unref (node);

  mojito_service_emit_capabilities_changed
    (service, get_dynamic_caps (service));

  if (priv->running)
    get_status_updates (facebook);
}

static void
got_user_cb (RestProxyCall *call,
             const GError  *error,
             GObject       *weak_object,
             gpointer       userdata)
{
  MojitoService *service = MOJITO_SERVICE (weak_object);
  MojitoServiceFacebook *facebook = MOJITO_SERVICE_FACEBOOK (service);
  MojitoServiceFacebookPrivate *priv = facebook->priv;
  RestXmlNode *node;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  node = node_from_call (call);
  if (!node)
    return;

  priv->uid = g_strdup (node->content);
  rest_xml_node_unref (node);

  if(!priv->uid)
    return;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "Users.getInfo");
  rest_proxy_call_add_params (call,
                              "uids", priv->uid,
                              "fields", "name, profile_url, pic_square",
                              NULL);

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback)got_user_info_cb,
                         (GObject*)service,
                         NULL,
                         NULL);
}

static void
got_tokens_cb (RestProxy *proxy, gboolean authorised, gpointer user_data)
{
  MojitoServiceFacebook *facebook = MOJITO_SERVICE_FACEBOOK (user_data);
  MojitoServiceFacebookPrivate *priv = facebook->priv;
  RestProxyCall *call;

  if (authorised) {
    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function (call, "users.getLoggedInUser");
    rest_proxy_call_async (call,
                           (RestProxyCallAsyncCallback)got_user_cb,
                           (GObject*)facebook,
                           NULL,
                           NULL);
  } else {
    mojito_service_emit_refreshed ((MojitoService *)facebook, NULL);
  }
}

static void
start (MojitoService *service)
{
  MojitoServiceFacebook *facebook = (MojitoServiceFacebook*)service;

  facebook->priv->running = TRUE;
}

static void
refresh (MojitoService *service)
{
  MojitoServiceFacebook *facebook = (MojitoServiceFacebook*)service;
  MojitoServiceFacebookPrivate *priv = facebook->priv;

  if (!priv->running)
    return;

  if (priv->uid == NULL) {
    mojito_keyfob_facebook ((FacebookProxy*)priv->proxy, got_tokens_cb, service);
  } else {
    get_status_updates (facebook);
  }
}

static void
_avatar_downloaded_cb (const gchar *uri,
                       gchar       *local_path,
                       gpointer     userdata)
{
  MojitoService *service = MOJITO_SERVICE (userdata);

  mojito_service_emit_avatar_retrieved (service, local_path);
  g_free (local_path);
}

static void
request_avatar (MojitoService *service)
{
  MojitoServiceFacebookPrivate *priv = GET_PRIVATE (service);

  if (priv->pic_square)
  {
    mojito_web_download_image_async (priv->pic_square,
                                     _avatar_downloaded_cb,
                                     service);
  } else {
    mojito_service_emit_avatar_retrieved (service, NULL);
  }
}

static void
_status_updated_cb (RestProxyCall *call,
                    const GError  *error,
                    GObject       *weak_object,
                    gpointer       userdata)
{
  MojitoService *service = MOJITO_SERVICE (weak_object);

  mojito_service_emit_status_updated (service, error == NULL);
}

static void
_got_permission_check_cb (RestProxyCall *call,
                          const GError  *error,
                          GObject       *weak_object,
                          gpointer       userdata)
{
  MojitoService *service = MOJITO_SERVICE (weak_object);
  MojitoServiceFacebook *facebook = MOJITO_SERVICE_FACEBOOK (service);
  MojitoServiceFacebookPrivate *priv = facebook->priv;
  RestXmlNode *node;
  gchar *status_msg = (gchar *)userdata;

  node = node_from_call (call);
  if (!node || !node->content)
    return;

  if (g_strcmp0(node->content, "0") == 0){
    /* TODO Launch Permission Request Page */
    rest_xml_node_unref (node);
    return;
  }
  rest_xml_node_unref (node);

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "Status.set");
  rest_proxy_call_add_param (call, "status", status_msg);

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback)_status_updated_cb,
                         (GObject *)service,
                         NULL,
                         NULL);
  g_free(status_msg);
}

static void
update_status (MojitoService *service, const char *msg)
{
  MojitoServiceFacebook *facebook = MOJITO_SERVICE_FACEBOOK (service);
  MojitoServiceFacebookPrivate *priv = facebook->priv;
  RestProxyCall *call;

  if (!priv->proxy)
    return;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "Users.hasAppPermission");
  rest_proxy_call_add_param (call, "ext_perm", "publish_stream");

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback)_got_permission_check_cb,
			 (GObject *)service,
			 (gpointer)g_strdup(msg),
			 NULL);
}

static const char *
mojito_service_facebook_get_name (MojitoService *service)
{
  return "facebook";
}

static void
online_notify (gboolean online, gpointer user_data)
{
  MojitoServiceFacebook *service = (MojitoServiceFacebook *) user_data;
  MojitoServiceFacebookPrivate *priv = service->priv;

  if (online) {
    const char *key = NULL, *secret = NULL;
    mojito_keystore_get_key_secret ("facebook", &key, &secret);
    priv->proxy = facebook_proxy_new (key, secret);
    mojito_keyfob_facebook ((FacebookProxy *)priv->proxy, got_tokens_cb, service);
  } else {
    mojito_service_emit_capabilities_changed ((MojitoService *)service, NULL);

    if (priv->proxy) {
      g_object_unref (priv->proxy);
      priv->proxy = NULL;
    }

    g_free (priv->uid);
    priv->uid = NULL;
  }
}

static void
_credentials_updated_func (gpointer data, gpointer userdata)
{
  MojitoService *service = MOJITO_SERVICE (data);
  MojitoServiceFacebookPrivate *priv = MOJITO_SERVICE_FACEBOOK (service)->priv;
     
  online_notify (FALSE, service);
  /* Clean up pic_square to prevent avatar retrieving */
  if (priv->pic_square){
     g_free (priv->pic_square);
     priv->pic_square = NULL;
  }
  online_notify (TRUE, service);

  mojito_service_emit_user_changed (service);
}

static void
credentials_updated (MojitoService *service)
{
  /* If we're online, force a reconnect to fetch new credentials */
  if (mojito_is_online ()) {
    g_list_foreach (service_list, _credentials_updated_func, NULL);
  }
}

static void
mojito_service_facebook_dispose (GObject *object)
{
  MojitoServiceFacebookPrivate *priv = MOJITO_SERVICE_FACEBOOK (object)->priv;

  service_list = g_list_remove (service_list, MOJITO_SERVICE_FACEBOOK (object));

  mojito_online_remove_notify (online_notify, object);

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (mojito_service_facebook_parent_class)->dispose (object);
}

static void
mojito_service_facebook_finalize (GObject *object)
{
  MojitoServiceFacebookPrivate *priv = MOJITO_SERVICE_FACEBOOK (object)->priv;

  g_free (priv->uid);

  G_OBJECT_CLASS (mojito_service_facebook_parent_class)->finalize (object);
}

static void
mojito_service_facebook_class_init (MojitoServiceFacebookClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoServiceClass *service_class = MOJITO_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoServiceFacebookPrivate));

  object_class->dispose = mojito_service_facebook_dispose;
  object_class->finalize = mojito_service_facebook_finalize;

  service_class->get_name = mojito_service_facebook_get_name;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->update_status = update_status;
  service_class->start = start;
  service_class->refresh = refresh;
  service_class->request_avatar = request_avatar;
  service_class->credentials_updated = credentials_updated;
}

static void
mojito_service_facebook_init (MojitoServiceFacebook *self)
{
  self->priv = GET_PRIVATE (self);

  service_list = g_list_append (service_list, self);

  if (mojito_is_online ()) {
    online_notify (TRUE, self);
  }

  mojito_online_add_notify (online_notify, self);
}
