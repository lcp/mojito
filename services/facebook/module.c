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

#include <mojito/mojito-module.h>
#include "facebook.h"

const gchar *
mojito_module_get_name (void)
{
  return "facebook";
}

const GType
mojito_module_get_type (void)
{
  return MOJITO_TYPE_SERVICE_FACEBOOK;
}
