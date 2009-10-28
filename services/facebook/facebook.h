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

#ifndef _MOJITO_SERVICE_FACEBOOK
#define _MOJITO_SERVICE_FACEBOOK

#include <mojito/mojito-service.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_SERVICE_FACEBOOK mojito_service_facebook_get_type()

#define MOJITO_SERVICE_FACEBOOK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_SERVICE_FACEBOOK, MojitoServiceFacebook))

#define MOJITO_SERVICE_FACEBOOK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_SERVICE_FACEBOOK, MojitoServiceFacebookClass))

#define MOJITO_IS_SERVICE_FACEBOOK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_SERVICE_FACEBOOK))

#define MOJITO_IS_SERVICE_FACEBOOK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_SERVICE_FACEBOOK))

#define MOJITO_SERVICE_FACEBOOK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_SERVICE_FACEBOOK, MojitoServiceFacebookClass))

typedef struct _MojitoServiceFacebookPrivate MojitoServiceFacebookPrivate;

typedef struct {
  MojitoService parent;
  MojitoServiceFacebookPrivate *priv;
} MojitoServiceFacebook;

typedef struct {
  MojitoServiceClass parent_class;
} MojitoServiceFacebookClass;

GType mojito_service_facebook_get_type (void);

G_END_DECLS

#endif /* _MOJITO_SERVICE_FACEBOOK */
