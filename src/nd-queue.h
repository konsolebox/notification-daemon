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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __ND_QUEUE_H
#define __ND_QUEUE_H

#include <glib-object.h>

#include "nd-notification.h"

G_BEGIN_DECLS

#define ND_TYPE_QUEUE         (nd_queue_get_type ())
#define ND_QUEUE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), ND_TYPE_QUEUE, NdQueue))
#define ND_QUEUE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), ND_TYPE_QUEUE, NdQueueClass))
#define ND_IS_QUEUE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ND_TYPE_QUEUE))
#define ND_IS_QUEUE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), ND_TYPE_QUEUE))
#define ND_QUEUE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), ND_TYPE_QUEUE, NdQueueClass))

typedef struct NdQueuePrivate NdQueuePrivate;

typedef struct
{
        GObject           parent;
        NdQueuePrivate *priv;
} NdQueue;

typedef struct
{
        GObjectClass   parent_class;

        void          (* changed) (NdQueue      *queue);
} NdQueueClass;

typedef enum
{
        ND_QUEUE_SUCCESS,
        ND_QUEUE_NONE_CLOSED,
        ND_QUEUE_CRITICAL_ERROR
} NdQueueResult;


GType               nd_queue_get_type                       (void);

NdQueue *           nd_queue_new                            (void);

guint               nd_queue_length                         (NdQueue        *queue);

NdNotification *    nd_queue_lookup                         (NdQueue        *queue,
                                                             guint           id);

void                nd_queue_add                            (NdQueue        *queue,
                                                             NdNotification *notification);
void                nd_queue_remove_for_id                  (NdQueue        *queue,
                                                             guint           id);

guint               nd_queue_count_open_notifications       (NdQueue        *queue);
NdQueueResult       nd_queue_close_oldest_open_notification (NdQueue        *queue);

G_END_DECLS

#endif /* __ND_QUEUE_H */
