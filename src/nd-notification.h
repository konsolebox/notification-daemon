/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ND_NOTIFICATION__
#define __ND_NOTIFICATION__ 1

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define ND_TYPE_NOTIFICATION (nd_notification_get_type ())
#define ND_NOTIFICATION(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), ND_TYPE_NOTIFICATION, NdNotification))
#define ND_IS_NOTIFICATION(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), ND_TYPE_NOTIFICATION))

typedef struct _NdNotification NdNotification;

typedef struct _NdNotificationClass
{
        GObjectClass parent_class;
} NdNotificationClass;

typedef enum
{
        ND_NOTIFICATION_CLOSED_EXPIRED = 1,
        ND_NOTIFICATION_CLOSED_USER = 2,
        ND_NOTIFICATION_CLOSED_API = 3,
        ND_NOTIFICATION_CLOSED_RESERVED = 4
} NdNotificationClosedReason;

GType                 nd_notification_get_type            (void) G_GNUC_CONST;

NdNotification *      nd_notification_new                 (const char     *sender);
gboolean              nd_notification_update              (NdNotification *notification,
                                                           const char     *app_name,
                                                           const char     *icon,
                                                           const char     *summary,
                                                           const char     *body,
                                                           const char    **actions,
                                                           GHashTable     *hints,
                                                           int             timeout);

gboolean              nd_notification_get_is_closed       (NdNotification *notification);
void                  nd_notification_get_update_time     (NdNotification *notification,
                                                           GTimeVal       *timeval);

guint                 nd_notification_get_id              (NdNotification *notification);
int                   nd_notification_get_timeout         (NdNotification *notification);
const char *          nd_notification_get_sender          (NdNotification *notification);
const char *          nd_notification_get_app_name        (NdNotification *notification);
const char *          nd_notification_get_icon            (NdNotification *notification);
const char *          nd_notification_get_summary         (NdNotification *notification);
const char *          nd_notification_get_body            (NdNotification *notification);
char **               nd_notification_get_actions         (NdNotification *notification);
GHashTable *          nd_notification_get_hints           (NdNotification *notification);

GdkPixbuf *           nd_notification_load_image          (NdNotification *notification,
                                                           int             size);

void                  nd_notification_close               (NdNotification *notification,
                                                           NdNotificationClosedReason reason);
void                  nd_notification_action_invoked      (NdNotification *notification,
                                                           const char     *action);
void                  nd_notification_url_clicked         (NdNotification *notification,
                                                           const char     *url);

G_END_DECLS

#endif
