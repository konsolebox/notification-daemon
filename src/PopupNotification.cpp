/**
 * @file popup-notifier.cpp GTK+ based popup notifier
 *
 * Copyright (C) 2004 Mike Hearn <mike@navi.cx>
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

#define GTK_DISABLE_DEPRECATED
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#ifndef _WIN32
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <X11/Xatom.h>
# include <gdk/gdkx.h>
#endif

#include <iostream>
#include <sstream>
#include <exception>
#include <stdexcept>

#include "notifier.h"
#include "logging.h"

#include <assert.h>

#define GTK_CALLBACK(name, params...) static gboolean name(GtkWidget *widget, params, gpointer user_data)

struct expose_data
{
    GtkWidget *widget;
    gulong handler_id;
};

class PopupNotification : public Notification    // FIXME: rename this to GtkPopupNotifier
{
private:
    static const int width = 300;     // FIXME: make these relative to screen size
    static const int minheight = 50;
    static const int imagesize = 48;
    static const int image_padding = 12;

	int disp_screen;

    GtkWidget *window;

    /* the contents of the popup. will be destroyed and rebuilt so we can do atomic replacements */
    GtkWidget *bodybox;

    int height_offset;

    void format_summary(GtkLabel *label)
    {
        PangoAttribute *bold = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
        PangoAttribute *large = pango_attr_scale_new(1.2);
        PangoAttrList *attrs = pango_attr_list_new();

        bold->start_index = large->start_index = 0;
        bold->end_index = large->end_index = G_MAXINT;

        pango_attr_list_insert(attrs, bold);
        pango_attr_list_insert(attrs, large);

        gtk_label_set_attributes(label, attrs);

        pango_attr_list_unref(attrs);

        /* the attributes aren't leaked, they are now owned by GTK  */
    }

    void process_body_markup(GtkLabel *label)
    {
        /* we can't use pango markup here because ... zzzzzzzzzzzzz  */
    }

    /* Make a label blue and underlined */
    void linkify(GtkLabel *label)
    {
        PangoAttribute *blue = pango_attr_foreground_new(0, 0, 65535);
        PangoAttribute *underline = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
        PangoAttrList *attrs = pango_attr_list_new();

        blue->start_index = underline->start_index = 0;
        blue->end_index = underline->end_index = G_MAXINT;

        pango_attr_list_insert(attrs, blue);
        pango_attr_list_insert(attrs, underline);

        gtk_label_set_attributes(label, attrs);

        pango_attr_list_unref(attrs);
    }

    void whiten(GtkWidget *eventbox)
    {
        GdkColor white;
        gdk_color_parse("white", &white);

        GtkStyle *style = gtk_widget_get_style(eventbox);   /* override the theme. white is GOOD dammit :) */
        style->bg[GTK_STATE_NORMAL] = white;

	gtk_widget_set_style(eventbox, style);
    }

    GdkGC *gc;

    GTK_CALLBACK( draw_border, GdkEventExpose *event )
    {
        PopupNotification *n = (PopupNotification *) user_data;

        if (!n->gc)
        {
            n->gc = gdk_gc_new(event->window);

            GdkColor color;
            gdk_color_parse("black", &color);
            gdk_gc_set_rgb_fg_color(n->gc, &color);
        }

        int w, h;
        gdk_drawable_get_size(event->window, &w, &h);

        gdk_draw_rectangle(event->window, n->gc, FALSE, 0, 0, w-1, h-1);

        return FALSE; /* propogate further */
    }

    PopupNotifier *notifier;

    void window_button_release(GdkEventButton *event)
    {
        if (actions.find(0) == actions.end()) notifier->unnotify(this);
        else action_invoke(0);
    }

    GTK_CALLBACK( _window_button_release, GdkEventButton *event )
    {
        ((PopupNotification *) user_data)->window_button_release(event);
        return TRUE;
    }

    GTK_CALLBACK( _action_label_click, GdkEventButton *event )
    {
        PopupNotification *n = (PopupNotification *) g_object_get_data(G_OBJECT(widget), "notification-instance");
        n->action_invoke( *((uint *)user_data) );
        return TRUE;
    }

    /* this ensures that the hyperlinks have a hand cursor. it's done
       in a callback because we have to wait for the window to be
       realized (ie for an X window to be created for the widget)  */

    GTK_CALLBACK( _set_cursor, GdkEventExpose *event )
    {
        struct expose_data *data = (struct expose_data *) user_data;

        GdkCursor *cursor = gdk_cursor_new(GDK_HAND2);
        gdk_window_set_cursor(event->window, cursor);
        gdk_cursor_unref(cursor);

        g_signal_handler_disconnect(data->widget, data->handler_id);

        delete data;

        return FALSE; /* propogate further  */
    }

public:

    PopupNotification(PopupNotifier *n)
		: Notification(),
	      disp_screen(0),
	      gc(NULL),
		  window(NULL),
		  height_offset(0),
		  notifier(n)
    {
    }

    ~PopupNotification()
    {
        TRACE("destroying notification %d, window=%p\n", id, window);

        if (gc) g_object_unref(gc);

        if (window)
        {
            gtk_widget_hide(window);
            gtk_widget_destroy(window);
            window = NULL;
        }
    }

    void generate()
    {
        TRACE("Generating PopupNotification GUI for nid %d\n", id);

        GtkWidget *win = window, *bodybox_widget, *vbox, *summary_label, *body_label, *image_widget;

        try
        {
            if (!window)
            {
                /* win will be assigned to window at the end */
                win = gtk_window_new(GTK_WINDOW_POPUP);

                gtk_widget_add_events(win, GDK_BUTTON_RELEASE_MASK);

                g_signal_connect(win, "button-release-event", G_CALLBACK(_window_button_release), this);
            }

            bodybox_widget = gtk_hbox_new(FALSE, 0);
            gtk_container_add(GTK_CONTAINER(win), bodybox_widget);
            gtk_widget_show(bodybox_widget);

            if (images.empty())
            {
                /* let's default to a nice generic bling! image :-)  */
                image_widget = gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
            }
            else
            {
                Image *image = images[0];

                if (image->type == IMAGE_TYPE_THEME)
                {
                    TRACE("new from icon theme: %s\n", image->file);

                    GtkIconTheme *theme = gtk_icon_theme_new();
                    GError *error = NULL;
                    GdkPixbuf *icon = gtk_icon_theme_load_icon(theme, image->file, imagesize,
                                                               GTK_ICON_LOOKUP_USE_BUILTIN, &error);
                    g_object_unref(G_OBJECT(theme));

                    if (!error)
                    {
                        image_widget = gtk_image_new_from_pixbuf(icon);
                        gdk_pixbuf_unref(icon);
                    }
                    else
                    {
                        throw std::runtime_error( S("could not load icon: ") + S(error->message) );
                    }
                }
                else if (image->type == IMAGE_TYPE_ABSOLUTE)
                {
                    TRACE("new from file: %s\n", image->file);

                    GError *error = NULL;
                    GdkPixbuf *buf = gdk_pixbuf_new_from_file(image->file, &error);

                    if (error)
                        throw std::runtime_error( S("could not load file: ") + S(error->message) );

                    image_widget = gtk_image_new_from_pixbuf(buf);

                    // do we leak buf here?
                }
                else if (image->type == IMAGE_TYPE_RAW)
                {
                    TRACE("new from raw: %c%c%c%c\n", image->data[0], image->data[1], image->data[2], image->data[3]);

                    GError *error = NULL;

                    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
                    if (error) throw std::runtime_error( S(error->message) );

                    gdk_pixbuf_loader_write(loader, image->data, image->datalen, &error);
                    gdk_pixbuf_loader_close(loader, NULL);
                    if (error)
                    {
                        g_object_unref(loader);
                        throw std::runtime_error( S(error->message) );
                    }

                    GdkPixbuf *buf = gdk_pixbuf_loader_get_pixbuf(loader);

                    image_widget = gtk_image_new_from_pixbuf(buf);

                    g_object_unref(loader);

                    // ditto, do we leak buf?
                }
                else
                {
                    std::ostringstream s;
                    s << "unhandled image type " << image->type;
                    throw std::runtime_error( s.str() );
                }

            }

            vbox = gtk_vbox_new(FALSE, 2);
            gtk_widget_show(vbox);

            /* now we want another hbox to provide some padding on the left, between
               the coloured part and the white part. in future it could also do a gradient
               or blend but for now we'll use a lame hack. there must be a better way to
               add padding to one side of a box in GTK.    */

            GtkWidget *padding = gtk_hbox_new(FALSE, 0);
            GtkWidget *padding_label = gtk_label_new("  ");
            gtk_widget_show(padding_label);
            gtk_box_pack_start_defaults(GTK_BOX(padding), padding_label);
            gtk_box_pack_end_defaults(GTK_BOX(padding), vbox);
            gtk_widget_show(padding);

            /* now set up the labels containing the notification text */
            summary_label = gtk_label_new(summary);
            format_summary(GTK_LABEL(summary_label));
            gtk_misc_set_alignment(GTK_MISC(summary_label), 0, 0.5);
            gtk_widget_show(summary_label);
            gtk_box_pack_start(GTK_BOX(vbox), summary_label, TRUE, TRUE, 5);

            if (body)
            {
                body_label = gtk_label_new(body);

                //process_body_markup(body_label);

                gtk_label_set_line_wrap(GTK_LABEL(body_label), TRUE);
                gtk_misc_set_alignment(GTK_MISC(body_label), 0, 0.5);
                gtk_widget_show(body_label);
                gtk_box_pack_start(GTK_BOX(vbox), body_label, TRUE, TRUE, 10);
            }

            /* we want to fix the width so the notifications expand upwards but not outwards.
               firstly, we need to grab the natural size request of the containing box, then we
               need to set the size request of the label to that width so it will always line wrap. */

            gtk_widget_set_size_request(body ? body_label : summary_label,
                                        width - (imagesize + image_padding) - 10 /* FIXME */, -1);

            summary_label = body_label = NULL;

            if (!actions.empty())
            {
                /*
				 * now let's do the actions. we'll show them as hyperlinks to
				 * save space, and because it looks cooler that way :)
				 */
                GtkWidget *actions_hbox = gtk_hbox_new(FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), actions_hbox, FALSE, FALSE, 5);
                gtk_widget_show(actions_hbox);

                foreach( ActionsMap, actions )
                {
                    TRACE("action %d is %s\n", i->first, i->second);

                    if (i->first == 0) continue;     /* skip the default action */

                    GtkWidget *eventbox = gtk_event_box_new();
                    gtk_box_pack_start(GTK_BOX(actions_hbox), eventbox, FALSE, FALSE, 0);

                    GtkWidget *label = gtk_label_new(i->second);
                    linkify(GTK_LABEL(label));
                    whiten(eventbox);
                    gtk_container_add(GTK_CONTAINER(eventbox), label);

                    g_signal_connect(G_OBJECT(eventbox), "button-release-event",
                                     G_CALLBACK(_action_label_click), (void *) &i->first);

                    g_object_set_data(G_OBJECT(eventbox), "notification-instance", this);

                    gtk_widget_show(label);

                    /* this will set the mouse cursor to be a hand on the hyperlinks  */
                    struct expose_data *data = new struct expose_data;
                    data->widget = eventbox;
                    data->handler_id = g_signal_connect(G_OBJECT(eventbox), "expose-event",
                                                        G_CALLBACK(_set_cursor), (void *) data);

                    /* if it's not the last item ...  */
                    if (i->second != actions[actions.size() - 1])
                    {
                        label = gtk_label_new(" | ");    /* ... add a separator */
                        gtk_box_pack_start(GTK_BOX(actions_hbox), label, FALSE, FALSE, 0);
                        gtk_widget_show(label);
                    }

                    gtk_widget_show(eventbox);
                }
            }

            /*
			 * set up an eventbox so we can get a white background. for some
			 * reason GTK insists that it be given a new X window if you want
			 * to change the background colour. it will just silently ignore
			 * requests to change the colours of eg, a box.
			 */

            GtkWidget *eventbox = gtk_event_box_new();
            whiten(eventbox);
            gtk_container_set_border_width(GTK_CONTAINER(eventbox), 1);   /* don't overdraw the black border */
            gtk_widget_show(eventbox);

            gtk_container_add(GTK_CONTAINER(eventbox), padding);
            gtk_box_pack_end_defaults(GTK_BOX(bodybox_widget), eventbox);

            GtkWidget *imagebox = gtk_vbox_new(FALSE, 0);
            gtk_container_set_border_width(GTK_CONTAINER(imagebox),
										   image_padding);

            if (image_widget)
            {
                gtk_widget_show(image_widget);
                gtk_box_pack_start_defaults(GTK_BOX(imagebox), image_widget);
				gtk_misc_set_alignment(GTK_MISC(image_widget), 0.5, 0.0);
                image_widget = NULL;
            }
            gtk_widget_show(imagebox);
            gtk_box_pack_start(GTK_BOX(bodybox_widget), imagebox, FALSE, FALSE, 0);

            /* now we setup an expose event handler to draw the border */
            g_signal_connect(win, "expose-event", G_CALLBACK(draw_border), this);
            gtk_widget_set_app_paintable(win, TRUE);

            gtk_widget_show(bodybox_widget);

            /* now we want to ensure the height of the content is never less than minheight */
            GtkRequisition req;
            gtk_widget_size_request(bodybox_widget, &req);
            gtk_widget_set_size_request(bodybox_widget, -1, MAX(minheight, req.height));
        }
        catch (...)
        {
            if (win) gtk_widget_destroy(win);   /* don't leak if an exception is thrown */
            throw;
        }

        /* now we have successfully built the UI, commit it to the instance  */
        this->bodybox = bodybox_widget;
        this->window = win;
        TRACE("window is %p\n", this->window);

        update_position();
    }

    void show()
    {
        if (!window) generate();    // can throw
        gtk_widget_show(window);
    }

    /* returns the natural height of the notification. generates the gui if not done so already */
    int height()
    {
        if (!window) generate();    // can throw

        GtkRequisition req;
        gtk_widget_size_request(window, &req);
        return req.height;
    }

    void update_position()
    {
        if (!window) generate();    // can throw

        GtkRequisition req;
        gtk_widget_size_request(window, &req);

		GdkRectangle workarea;

		if (!get_work_area(workarea))
		{
			workarea.width  = gdk_screen_width();
			workarea.height = gdk_screen_height();
		}

        gtk_window_move(GTK_WINDOW(window),
                        workarea.x + workarea.width - req.width,
                        workarea.y + workarea.height - height() - height_offset);
    }

	bool get_work_area(GdkRectangle &rect)
	{
#ifndef _WIN32
		Atom workarea = XInternAtom(GDK_DISPLAY(), "_NET_WORKAREA", True);

		if (workarea == None)
			return false;

		Window win = XRootWindow(GDK_DISPLAY(), disp_screen);

		Atom type;
		gint format;
		gulong num, leftovers;
		gulong max_len = 4 * 32;
		guchar *ret_workarea;
		gint result = XGetWindowProperty(GDK_DISPLAY(), win, workarea, 0,
										 max_len, False, AnyPropertyType,
										 &type, &format, &num,
										 &leftovers, &ret_workarea);

		if (result != Success || type == None || format == 0 ||
			leftovers || num % 4)
		{
			return false;
		}

		guint32 *workareas = (guint32 *)ret_workarea;

		rect.x      = workareas[disp_screen * 4];
		rect.y      = workareas[disp_screen * 4 + 1];
		rect.width  = workareas[disp_screen * 4 + 2];
		rect.height = workareas[disp_screen * 4 + 3];

		XFree(ret_workarea);

		return true;
#else /* _WIN32 */
		return false;
#endif /* _WIN32 */
	}

    void set_height_offset(int value)
    {
        height_offset = value;
        update_position();     // can throw
    }

    void update()
    {
        /* contents have changed, so scrap current UI and regenerate */
        TRACE("updating for %d\n", id);

        if (window)
        {
            gtk_container_remove(GTK_CONTAINER(window), bodybox);
            bodybox = NULL;
        }

        generate();    // can throw
    }
};

PopupNotifier::PopupNotifier(GMainLoop *main_loop, int *argc, char ***argv) : BaseNotifier(main_loop)
{
    gtk_init(argc, argv);
}

/* This method is responsible for calculating the height offsets of all currently
   displayed notifications. In future, it may take into account animations and such.

   This may be called many times per second so it should be reasonably fast. */

void PopupNotifier::reflow()
{
    /* the height offset is the distance from the top/bottom of the
       screen to the nearest edge of the popup */

    int offset = 0;
	int offsub = 0;

    foreach( NotificationsMap, notifications )
    {
        PopupNotification *n = dynamic_cast<PopupNotification*> (i->second);
        assert( n != NULL );

        n->set_height_offset(offset - offsub);

        offset += n->height();
		offsub++;
    }
}

uint PopupNotifier::notify(Notification *base)
{
    PopupNotification *n = dynamic_cast<PopupNotification*> (base);
    assert( n != NULL );

    uint id = BaseNotifier::notify(base);  // can throw

    reflow();

    n->show();

    return id;
}

bool PopupNotifier::unnotify(Notification *n)
{
    bool ret = BaseNotifier::unnotify(n);

    reflow();

    return ret;
}

Notification* PopupNotifier::create_notification()
{
    return new PopupNotification(this);
}

