/*
 *  xfce4-taskmanager - very simple taskmanger
 *
 *  Copyright (c) 2006 Johannes Zellner, <webmaster@nebulon.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "callbacks.h"

extern guint rID;

void on_button3_toggled_event(GtkButton *button, GdkEventButton *event)
{
    full_view = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
    change_list_store_view();
}

gboolean on_treeview1_button_press_event(GtkButton *button, GdkEventButton *event)
{
    if(event->button == 3)
    {
        GdkEventButton *mouseevent = (GdkEventButton *)event;
        gtk_menu_popup(GTK_MENU(taskpopup), NULL, NULL, NULL, NULL, mouseevent->button, mouseevent->time);
    }
    return FALSE;
}

static void set_menu_position(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data)
{
    GtkWidget *widget = GTK_WIDGET(user_data);
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
        GtkTreeView *treeview = GTK_TREE_VIEW(widget);
        // scroll to selected line to make it visible in more cases
        gtk_tree_view_scroll_to_cell(treeview, path, NULL, FALSE, 0, 0);
        // get bin window coordinates of selected name column
        GdkRectangle rect;
        gtk_tree_view_get_cell_area(treeview, path, gtk_tree_view_get_column(treeview, COLUMN_NAME), &rect);
        gtk_tree_path_free(path);
        // convert bin window coordinates to widget relative coordinates
        gint dx;
        gint dy;
        gtk_tree_view_convert_bin_window_to_widget_coords(treeview, rect.x, rect.y, &dx, &dy);
        // get tree position
        gint tree_x;
        gint tree_y;
        gdk_window_get_origin(widget->window, &tree_x, &tree_y);
        // add relative coordinates of cell center to widget coordinates
        gint menu_x = tree_x + dx + rect.width / 2;
        gint menu_y = tree_y + dy + rect.height / 2;
        // get tree size
        GdkRectangle tree_rect;
        gtk_tree_view_get_visible_rect(treeview, &tree_rect);
        // get menu size
        GtkRequisition menu_size;
        gtk_widget_size_request(GTK_WIDGET(menu), &menu_size);
        // force context menu not right of tree
        if (menu_x + menu_size.width > tree_x + tree_rect.width) {
            menu_x = tree_x + tree_rect.width - menu_size.width;
        }
        // force context menu not left of tree
        if (menu_x < tree_x) {
            menu_x = tree_x;
        }
        // force context menu not below bottom
        gint screen_height = gdk_screen_get_height(gtk_widget_get_screen(GTK_WIDGET(menu)));
        if (menu_y + menu_size.height > screen_height) {
            // try move context menu up
            menu_y -= menu_size.height;
            if (menu_y + menu_size.height > screen_height) {
                menu_y = screen_height - menu_size.height;
            }
        }
        // force context menu not above top
        if (menu_y < 0) {
            menu_y = 0;
        }
        // return position of context menu
        *x = menu_x;
        *y = menu_y;
        // move context menu fully into visible area horizontally
        *push_in = TRUE;
    }
}

gboolean on_treeview_popup_menu(GtkWidget *widget, gpointer user_data) {
    gtk_menu_popup(GTK_MENU(taskpopup), NULL, NULL, (GtkMenuPositionFunc) set_menu_position, widget, 0, gdk_event_get_time(NULL));
    return TRUE;
}

void on_focus_in_event(GtkWidget * widget, GdkEvent *event, gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        GtkTreePath *path;
        GtkTreeViewColumn *focus_column;
        gtk_tree_view_get_cursor(GTK_TREE_VIEW(widget), &path, &focus_column);
        if (NULL != path) {
            gtk_tree_selection_select_path(selection, path);
            gtk_tree_path_free(path);
        } else {
            GtkTreeIter iter;
            gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list_store), &iter);
            gtk_tree_selection_select_iter(selection, &iter);
        }
    }
}

void handle_task_menu(GtkWidget *widget, gchar *signal)
{
    if(signal != NULL)
    {
        gint task_action = SIGNAL_NO;

        switch(signal[0])
        {
            case 'K':
                if(confirm(_("Really kill the task?"), FALSE))
                    task_action = SIGNAL_KILL;
                break;
            case 'T':
                if(confirm(_("Really terminate the task?"), TRUE))
                    task_action = SIGNAL_TERM;
                break;
            case 'S':
                task_action = SIGNAL_STOP;
                break;
            case 'C':
                task_action = SIGNAL_CONT;
                break;
            default:
                return;
        }

        if(task_action != SIGNAL_NO)
        {
            gchar *task_id = "";
            GtkTreeModel *model;
            GtkTreeIter iter;

            if(gtk_tree_selection_get_selected(selection, &model, &iter))
            {
                gtk_tree_model_get(model, &iter, COLUMN_PID, &task_id, -1);
                if(atoi(task_id)==getpid() && task_action==SIGNAL_STOP)
                {
                    show_error(_("Can't stop process self"));
                }
                else
                {
                    send_signal_to_task(atoi(task_id), task_action);
                    refresh_task_list();
                }
            }
        }
    }
}

void handle_prio_menu(GtkWidget *widget, gchar *prio)
{
    gchar *task_id = "";
    GtkTreeModel *model;
    GtkTreeIter iter;

    if(gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        gtk_tree_model_get(model, &iter, 1, &task_id, -1);

        set_priority_to_task(atoi(task_id), atoi(prio));
        refresh_task_list();
    }
}

void on_show_tasks_toggled (GtkMenuItem *menuitem, gpointer data)
{
    gssize uid = (gssize)data;

    if(uid == (gssize)own_uid)
        show_user_tasks = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));
    else if(uid == 0)
        show_root_tasks = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));
    else if(uid == -1)
        show_other_tasks = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));
    else {
	
	show_full_path = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));
        change_full_path();
        return;
    }
    
    change_task_view();
}

void on_show_cached_as_free_toggled (GtkMenuItem *menuitem, gpointer data)
{
    show_cached_as_free = !show_cached_as_free;
    change_task_view();
}

void on_quit(void)
{
    g_source_remove(rID);
    save_config();
    free(config_file);

    gtk_main_quit();
}
