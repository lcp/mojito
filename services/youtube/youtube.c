/*
 * Mojito - social data store
 * Copyright (C) 2010 Novell Inc.
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
#include <stdlib.h>
#include <string.h>
#include "youtube.h"
#include <glib/gi18n.h>
#include <mojito/mojito-service.h>
#include <mojito/mojito-item.h>
#include <mojito/mojito-utils.h>
#include <mojito/mojito-web.h>
#include <mojito/mojito-call-list.h>
#include <mojito-keystore/mojito-keystore.h>
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <gconf/gconf-client.h>

G_DEFINE_TYPE (MojitoServiceYoutube, mojito_service_youtube, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_YOUTUBE, MojitoServiceYoutubePrivate))

#define KEY_BASE "/apps/mojito/services/youtube"
#define KEY_USER KEY_BASE "/user"

struct _MojitoServiceYoutubePrivate {
  gboolean running;
  RestProxy *proxy;
  GConfClient *gconf;
  char *user_id;
  guint gconf_notify_id;
  MojitoSet *set;
  MojitoCallList *calls;
};

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
    g_message ("Error from youtube: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  if (root == NULL) {
    g_message ("Error from Youtube: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  /*TODO review THIS*/
  if (strcmp (root->name, "error_response") == 0) {
    RestXmlNode *node;
    node = rest_xml_node_find (root, "error_msg");
    g_message ("Error response from Youtube: %s\n", node->content);
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


static void
emit_if_done (MojitoServiceYoutube *youtube)
{
  if (mojito_call_list_is_empty (youtube->priv->calls)) {
    mojito_service_emit_refreshed ((MojitoService *)youtube, youtube->priv->set);
    mojito_set_empty (youtube->priv->set);
  }
}

static void
start (MojitoService *service)
{
  MojitoServiceYoutube *youtube = MOJITO_SERVICE_YOUTUBE (service);

  youtube->priv->running = TRUE;
}

static void
got_video_list_cb (RestProxyCall *call,
                   const GError  *error,
                   GObject       *weak_object,
                   gpointer       user_data)
{
  MojitoService *service = MOJITO_SERVICE (weak_object);
  MojitoServiceYoutubePrivate *priv = MOJITO_SERVICE_YOUTUBE (service)->priv;
  RestXmlNode *root, *node;
  MojitoSet *set;

  root = node_from_call (call);
  if (!root)
    return;

  node = rest_xml_node_find (node, "channel");
  if (!node)
    return;

  node = rest_xml_node_find (node, "item");

  set = mojito_item_set_new ();

  while (node){
    /*
    <rss>
      <channel>
        <item>
	  <guid isPermaLink="false">http://gdata.youtube.com/feeds/api/videos/<videoid></guid>
          <atom:updated>2010-02-13T06:17:32.000Z</atom:updated>
	  <title>Video Title</title>
	  <author>Author Name</author>
	  <link>http://www.youtube.com/watch?v=<videoid>&amp;feature=youtube_gdata</link>
	  <media:group>
	    <media:thumbnail url="http://i.ytimg.com/vi/<videoid>/default.jpg" height="90" width="120" time="00:03:00.500"/>
	  </media:group>
        </item>
      </channel>
    </rss>
    */
    MojitoItem *item;
    char *thumbnail;
    RestXmlNode *subnode, *thumb_node, *video_node;

    item = mojito_item_new ();
    mojito_item_set_service (item, service);

    mojito_item_put (item, "id", get_child_node_value (node, "guid"));
    mojito_item_put (item, "date", get_child_node_value (node, "atom:updated"));
    mojito_item_put (item, "title", get_child_node_value (node, "title"));
    mojito_item_put (item, "author", get_child_node_value (node, "author"));
    mojito_item_put (item, "url", get_child_node_value (node, "link"));

    /* media:group */
    subnode = rest_xml_node_find (node, "media:group");
    if (subnode){
      thumb_node = rest_xml_node_find (subnode, "media:thumbnail");
      thumbnail = rest_xml_node_get_attr (thumb_node, "url");
      mojito_item_request_image_fetch (item, "thumbnail", thumbnail);
    }

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
refresh (MojitoService *service)
{
  MojitoServiceYoutube *youtube = MOJITO_SERVICE_YOUTUBE (service);
  RestProxyCall *call;
  char *function;

  if (!youtube->priv->running || youtube->priv->user_id == NULL) {
    return;
  }

  mojito_call_list_cancel_all (youtube->priv->calls);
  mojito_set_empty (youtube->priv->set);

  call = rest_proxy_new_call (youtube->priv->proxy);
  mojito_call_list_add (youtube->priv->calls, call);
  function = g_strdup_printf ("users/%s/newsubscriptionvideos", youtube->priv->user_id);
  rest_proxy_call_set_function (call, function);
  rest_proxy_call_add_params (call,
                              "max-results", "10",
                              "alt", "rss",
                              NULL);
  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback)got_video_list_cb,
                         (GObject*)service,
                         NULL,
                         NULL);
  g_free (function);
}

static void
user_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  MojitoService *service = MOJITO_SERVICE (user_data);
  MojitoServiceYoutube *youtube = MOJITO_SERVICE_YOUTUBE (service);
  MojitoServiceYoutubePrivate *priv = youtube->priv;
  const char *new_user;

  if (entry->value) {
    new_user = gconf_value_get_string (entry->value);
    if (new_user && new_user[0] == '\0')
      new_user = NULL;
  } else {
    new_user = NULL;
  }

  if (g_strcmp0 (new_user, priv->user_id) != 0) {
    g_free (priv->user_id);
    priv->user_id = g_strdup (new_user);

    if (priv->user_id)
      refresh (service);
    else
      mojito_service_emit_refreshed (service, NULL);
  }
}

static const char *
get_name (MojitoService *service)
{
  return "youtube";
}

static void
mojito_service_youtube_dispose (GObject *object)
{
  MojitoServiceYoutubePrivate *priv = ((MojitoServiceYoutube*)object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->gconf) {
    gconf_client_notify_remove (priv->gconf,
                                priv->gconf_notify_id);
    gconf_client_remove_dir (priv->gconf, KEY_BASE, NULL);
    g_object_unref (priv->gconf);
    priv->gconf = NULL;
  }

  /* Do this here so only disposing if there are callbacks pending */
  if (priv->calls) {
    mojito_call_list_free (priv->calls);
    priv->calls = NULL;
  }

  G_OBJECT_CLASS (mojito_service_youtube_parent_class)->dispose (object);
}

static void
mojito_service_youtube_finalize (GObject *object)
{
  MojitoServiceYoutubePrivate *priv = ((MojitoServiceYoutube*)object)->priv;

  g_free (priv->user_id);

  mojito_set_unref (priv->set);

  G_OBJECT_CLASS (mojito_service_youtube_parent_class)->finalize (object);
}

static void
mojito_service_youtube_class_init (MojitoServiceYoutubeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoServiceClass *service_class = MOJITO_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoServiceYoutubePrivate));

  object_class->dispose = mojito_service_youtube_dispose;
  object_class->finalize = mojito_service_youtube_finalize;

  service_class->get_name = get_name;
  service_class->start = start;
  service_class->refresh = refresh;
}

static void
mojito_service_youtube_init (MojitoServiceYoutube *self)
{
  MojitoServiceYoutubePrivate *priv;

  priv = self->priv = GET_PRIVATE (self);

  priv->set = mojito_item_set_new ();
  priv->calls = mojito_call_list_new ();

  priv->running = FALSE;

  priv->proxy = rest_proxy_new ("http://gdata.youtube.com/feeds/api/", FALSE);

  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, KEY_BASE,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  priv->gconf_notify_id = gconf_client_notify_add (priv->gconf, KEY_USER,
                                                   user_changed_cb, self,
                                                   NULL, NULL);
  gconf_client_notify (priv->gconf, KEY_USER);
}
