/** -*- mode: c-mode; tab-width: 4; indent-tabs-mode: t; -*-
 * @file notifier.cpp Base class implementations
 *
 * Copyright (C) 2004 Mike Hearn
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 */

#include <glib.h> // for GMainLoop
#include <time.h>

#include "notifier.h"
#include "logging.h"

Notification::Notification()
{
	summary = body = sound = NULL;
	images = NULL;
	primary_frame = -1;
	timeout = 0;
	use_timeout = false;
	id = 0;
}

Notification::~Notification()
{
	if (summary) free(summary);
	if (body) free(body);
	// FIXME: free images/sound data
}

/*************************************************************/

BaseNotifier::BaseNotifier(GMainLoop *main_loop)
{
	loop = main_loop;
	g_main_loop_ref(loop);
	
	next_id = 0;
	timing = false;
}

BaseNotifier::~BaseNotifier()
{
	g_main_loop_unref(loop);
}

bool BaseNotifier::timeout()
{
	/* check each notification to see if it timed out yet */
	NotificationsMap::iterator i = notifications.begin();
	time_t now = time(NULL);

	while (i != notifications.end()) {
		if (i->second->timeout <= now) unnotify(i->second);
		i++;
	}

	TRACE("heartbeat: %d, %d notifications left\n", now, notifications.size());
	
	return !notifications.empty();
}

static gboolean timeout_dispatch(gpointer data)
{
	BaseNotifier *n = (BaseNotifier *) data;
	
	bool ret = n->timeout();
	if (!ret) n->timing = false;
	
	return ret ? TRUE : FALSE;
}

void BaseNotifier::register_timeout(int hz)
{
	g_timeout_add(hz, (GSourceFunc) timeout_dispatch, this);
}

uint BaseNotifier::notify(Notification *n)
{
	/* add to the internal list using the next cookie, increment, then
	   register a timeout, once per second if one isn't already
	   registered to count down.

	   we don't have a timeout triggering constantly as otherwise n-d
	   could never be fully paged out by the kernel.
	 */
	
	n->id = next_id;
	
	next_id++;
	
	notifications[n->id] = n;

	/* decide a sensible timeout. for now let's just use 5 seconds. in future, based on text length? */
	if (n->timeout == 0) n->timeout = time(NULL) + 5;
	
	if (n->use_timeout && !timing) {
		register_timeout(1000);
		timing = true;
	}
	
	return n->id;
}

bool BaseNotifier::unnotify(uint id)
{
	Notification *n = get(id);
	
	validate( n != NULL, false,
			  "Given ID (%d) is not valid", id );
	
	return unnotify(n);
}

bool BaseNotifier::unnotify(Notification *n)
{
	if (!notifications.erase(n->id)) {
		WARN("no such notification (%p)\n", n);
		return false;
	}

	TRACE("deleting due to unnotify (%p)\n", n);
	delete n;
	
	return true;
}

Notification* BaseNotifier::create_notification()
{
	return new Notification();
}

Notification *BaseNotifier::get(uint id)
{
	if (notifications.find(id) == notifications.end()) return NULL;
	return notifications[id];
}
