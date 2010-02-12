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

#ifndef _MOJITO_SERVICE_YOUTUBE
#define _MOJITO_SERVICE_YOUTUBE

#include <glib-object.h>
#include <mojito/mojito-service.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_SERVICE_YOUTUBE mojito_service_youtube_get_type()

#define MOJITO_SERVICE_YOUTUBE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_SERVICE_YOUTUBE, MojitoServiceYoutube))

#define MOJITO_SERVICE_YOUTUBE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_SERVICE_YOUTUBE, MojitoServiceYoutubeClass))

#define MOJITO_IS_SERVICE_YOUTUBE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_SERVICE_YOUTUBE))

#define MOJITO_IS_SERVICE_YOUTUBE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_SERVICE_YOUTUBE))

#define MOJITO_SERVICE_YOUTUBE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_SERVICE_YOUTUBE, MojitoServiceYoutubeClass))

typedef struct _MojitoServiceYoutubePrivate MojitoServiceYoutubePrivate;

typedef struct {
  MojitoService parent;
  MojitoServiceYoutubePrivate *priv;
} MojitoServiceYoutube;

typedef struct {
  MojitoServiceClass parent_class;
} MojitoServiceYoutubeClass;

GType mojito_service_youtube_get_type (void);

G_END_DECLS

#endif /* _MOJITO_SERVICE_YOUTUBE */
