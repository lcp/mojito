/*
 * Mojito Facebook service support
 * Copyright (C) 2009 Novell, Inc.
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

#include <string.h>
#include <mojito-keyfob/mojito-keyfob.h>
#include <rest/facebook-proxy.h>
#include <gnome-keyring.h>

#define FACEBOOK_SERVER "http://facebook.com/"

static const GnomeKeyringPasswordSchema facebook_schema = {
  GNOME_KEYRING_ITEM_GENERIC_SECRET,
  {
    { "server", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "api-key", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { NULL, 0 }
  }
};

typedef struct {
  FacebookProxy *proxy;
  MojitoKeyfobCallback callback;
  gpointer user_data;
} CallbackData;

/*
 * Split @string on whitespace into two strings and base-64 decode them.
 */
static gboolean
decode (const char *string, char **token, char **token_secret)
{
  char **encoded_keys;
  gboolean ret = FALSE;
  gsize len;

  g_assert (string);
  g_assert (token);
  g_assert (token_secret);

  encoded_keys = g_strsplit (string, " ", 2);

  if (encoded_keys[0] && encoded_keys[1]) {
    *token = (char*)g_base64_decode (encoded_keys[0], &len);
    *token_secret = (char*)g_base64_decode (encoded_keys[1], &len);
    ret = TRUE;
  }

  g_strfreev (encoded_keys);

  return ret;
}

//#if 0
static void
callback_data_free (CallbackData *data)
{
  g_object_unref (data->proxy);
  g_slice_free (CallbackData, data);
}

/*
 * Callback from gnome-keyring with the result of looking up the server/key
 * pair.  If this returns a secret then we can decode it and callback, otherwise
 * we have to ask the user to authenticate.
 */
static void
find_facebook_key_cb (GnomeKeyringResult result,
             const char *string,
             gpointer user_data)
{
  CallbackData *data = user_data;

  if (result == GNOME_KEYRING_RESULT_OK) {
    char *session = NULL, *app_secret = NULL;

    if (decode (string, &session, &app_secret)) {
      /*
       * TODO: is it possible to validate these tokens generically? If so then
       * it should be validated here, otherwise we need a way to clear the
       * tokens for a particular key so that re-auth works.
       */
      facebook_proxy_set_session_key (data->proxy, session);
      facebook_proxy_set_app_secret (data->proxy, app_secret);

      g_free (session);
      g_free (app_secret);

      data->callback ((RestProxy*)data->proxy, TRUE, data->user_data);
    } else {
      data->callback ((RestProxy*)data->proxy, FALSE, data->user_data);
    }
  } else {
    data->callback ((RestProxy*)data->proxy, FALSE, data->user_data);
  }

  /* Cleanup of data is done by gnome-keyring, bless it */
}

void
mojito_keyfob_facebook (FacebookProxy *proxy,
                          MojitoKeyfobCallback callback,
                          gpointer user_data)
{
  const char *key;
  CallbackData *data;

  /* TODO: hacky, make a proper singleton or proper object */
  data = g_slice_new0 (CallbackData);
  data->proxy = g_object_ref (proxy);
  data->callback = callback;
  data->user_data = user_data;

  key = facebook_proxy_get_api_key (proxy);

  gnome_keyring_find_password (&facebook_schema,
                               find_facebook_key_cb,
                               data, (GDestroyNotify)callback_data_free,
                               "server", FACEBOOK_SERVER,
                               "api-key", key,
                               NULL);
}
//#endif

gboolean
mojito_keyfob_facebook_sync (FacebookProxy *proxy)
{
  const char *key;
  char *password = NULL;
  GnomeKeyringResult result;

  key = facebook_proxy_get_api_key (proxy);

  result = gnome_keyring_find_password_sync (&facebook_schema, &password,
                                             "server", FACEBOOK_SERVER,
                                             "api-key", key,
                                             NULL);

  if (result == GNOME_KEYRING_RESULT_OK) {
    char *session = NULL, *secret = NULL;
    if (decode (password, &session, &secret)) {
      facebook_proxy_set_app_secret (proxy, secret);
      facebook_proxy_set_session_key (proxy, session);

      g_free (session);
      g_free (secret);
      gnome_keyring_free_password (password);

      return TRUE;
    } else {
      gnome_keyring_free_password (password);
      return FALSE;
    }
  } else {
    return FALSE;
  }
}
