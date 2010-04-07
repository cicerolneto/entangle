/*
 *  Entangle: Entangle Assists Photograph Aquisition
 *
 *  Copyright (C) 2009 Daniel P. Berrange
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <unique/unique.h>

#include "entangle-debug.h"
#include "entangle-app-display.h"
#include "entangle-camera-picker.h"
#include "entangle-camera-manager.h"

#define ENTANGLE_APP_DISPLAY_GET_PRIVATE(obj)                               \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), ENTANGLE_TYPE_APP_DISPLAY, EntangleAppDisplayPrivate))

struct _EntangleAppDisplayPrivate {
    UniqueApp *unique;

    EntangleCameraPicker *picker;
    EntangleCameraManager *manager;
};

G_DEFINE_TYPE(EntangleAppDisplay, entangle_app_display, ENTANGLE_TYPE_APP);


static void entangle_app_display_finalize (GObject *object)
{
    EntangleAppDisplay *display = ENTANGLE_APP_DISPLAY(object);
    EntangleAppDisplayPrivate *priv = display->priv;

    ENTANGLE_DEBUG("Finalize display");

    g_object_unref(priv->unique);
    g_object_unref(priv->picker);

    G_OBJECT_CLASS (entangle_app_display_parent_class)->finalize (object);
}

static void entangle_app_display_class_init(EntangleAppDisplayClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = entangle_app_display_finalize;

    g_signal_new("app-closed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(EntangleAppDisplayClass, app_closed),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);

    g_type_class_add_private(klass, sizeof(EntangleAppDisplayPrivate));
}


EntangleAppDisplay *entangle_app_display_new(void)
{
    return ENTANGLE_APP_DISPLAY(g_object_new(ENTANGLE_TYPE_APP_DISPLAY, NULL));
}


static void do_picker_close(EntangleCameraPicker *picker, EntangleAppDisplay *display G_GNUC_UNUSED)
{
    entangle_camera_picker_hide(picker);
}


static void do_picker_refresh(EntangleCameraPicker *picker G_GNUC_UNUSED, EntangleAppDisplay *display)
{
    entangle_app_refresh_cameras(ENTANGLE_APP(display));
}

static void do_manager_connect(EntangleCameraManager *manager G_GNUC_UNUSED,
                               EntangleAppDisplay *display)
{
    entangle_app_display_show(display);
}


static void do_picker_connect(EntangleCameraPicker *picker, EntangleCamera *cam, EntangleAppDisplay *display)
{
    EntangleAppDisplayPrivate *priv = display->priv;
    ENTANGLE_DEBUG("emit connect %p %s", cam, entangle_camera_get_model(cam));

    while (!entangle_camera_connect(cam)) {
        int response;
        GtkWidget *msg = gtk_message_dialog_new(NULL,
                                                GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_NONE,
                                                "%s",
                                                "Unable to connect to camera");

        gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(msg),
                                                   "%s",
                                                   "Check that the camera is not\n\n"
                                                   " - opened by another photo <b>application</b>\n"
                                                   " - mounted as a <b>filesystem</b> on the desktop\n"
                                                   " - in <b>sleep mode</b> to save battery power\n");

        gtk_dialog_add_button(GTK_DIALOG(msg), "Cancel", GTK_RESPONSE_CANCEL);
        gtk_dialog_add_button(GTK_DIALOG(msg), "Retry", GTK_RESPONSE_ACCEPT);

        response = gtk_dialog_run(GTK_DIALOG(msg));

        gtk_widget_hide(msg);
        //g_object_unref(msg);

        if (response == GTK_RESPONSE_CANCEL)
            return;
    }

    entangle_camera_manager_set_camera(priv->manager, cam);
    entangle_camera_picker_hide(picker);
}

static void do_set_icons(void)
{
    GList *icons = NULL;
    int iconSizes[] = { 16, 32, 48, 64, 128, 0 };

    for (int i = 0 ; iconSizes[i] != 0 ; i++) {
        char *local = g_strdup_printf("./entangle-%dx%d.png", iconSizes[i], iconSizes[i]);
        if (access(local, R_OK) < 0) {
            g_free(local);
            local = g_strdup_printf("%s/entangle-%dx%d.png", PKGDATADIR, iconSizes[i], iconSizes[i]);
        }
        GdkPixbuf *pix = gdk_pixbuf_new_from_file(local, NULL);
        if (pix)
            icons = g_list_append(icons, pix);
    }

    gtk_window_set_default_icon_list(icons);
}

static UniqueResponse
do_unique_message(UniqueApp *app G_GNUC_UNUSED,
                  UniqueCommand command,
                  UniqueMessageData *message_data G_GNUC_UNUSED,
                  guint time_ms G_GNUC_UNUSED,
                  gpointer data)
{
    EntangleAppDisplay *display = ENTANGLE_APP_DISPLAY(data);
    EntangleAppDisplayPrivate *priv = display->priv;

    if (command == UNIQUE_ACTIVATE) {
        entangle_camera_manager_show(priv->manager);
    }
    return UNIQUE_RESPONSE_OK;
}

static void do_camera_removed(EntangleCameraList *cameras G_GNUC_UNUSED,
                              EntangleCamera *camera,
                              gpointer data)
{
    EntangleAppDisplay *display = ENTANGLE_APP_DISPLAY(data);
    EntangleAppDisplayPrivate *priv = display->priv;
    EntangleCamera *current;

    g_object_get(priv->manager, "camera", &current, NULL);

    ENTANGLE_DEBUG("Check removed camera '%s' %p, against '%s' %p",
               entangle_camera_get_model(camera), camera,
               current ? entangle_camera_get_model(current) : "<none>", current);

    if (current == camera)
        entangle_camera_manager_set_camera(priv->manager, NULL);
}

static void entangle_app_display_init(EntangleAppDisplay *display)
{
    EntangleAppDisplayPrivate *priv;
    EntangleCameraList *cameras;

    priv = display->priv = ENTANGLE_APP_DISPLAY_GET_PRIVATE(display);

    do_set_icons();

    priv->picker = entangle_camera_picker_new(entangle_app_get_cameras(ENTANGLE_APP(display)));
    priv->manager = entangle_camera_manager_new(entangle_app_get_preferences(ENTANGLE_APP(display)),
                                            entangle_app_get_plugin_manager(ENTANGLE_APP(display)));

    cameras = entangle_app_get_cameras(ENTANGLE_APP(display));

    g_signal_connect(priv->picker, "picker-close", G_CALLBACK(do_picker_close), display);
    g_signal_connect(priv->picker, "picker-refresh", G_CALLBACK(do_picker_refresh), display);
    g_signal_connect(priv->picker, "picker-connect", G_CALLBACK(do_picker_connect), display);

    g_signal_connect(cameras, "camera-removed", G_CALLBACK(do_camera_removed), display);
    g_signal_connect(priv->manager, "manager-connect",
                     G_CALLBACK(do_manager_connect), display);

    priv->unique = unique_app_new("org.entangle_project.Display", NULL);
    g_signal_connect(priv->unique, "message-received",
                     G_CALLBACK(do_unique_message), display);
}


gboolean entangle_app_display_show(EntangleAppDisplay *display)
{
    EntangleAppDisplayPrivate *priv = display->priv;
    EntangleCameraList *cameras;
    gboolean choose = TRUE;

    if (unique_app_is_running(priv->unique)) {
        unique_app_send_message(priv->unique, UNIQUE_ACTIVATE, NULL);
        return FALSE;
    }

    cameras = entangle_app_get_cameras(ENTANGLE_APP(display));

    if (entangle_camera_list_count(cameras) == 1) {
        EntangleCamera *cam = entangle_camera_list_get(cameras, 0);

        if (entangle_camera_connect(cam)) {
            entangle_camera_manager_set_camera(priv->manager, cam);
            choose = FALSE;
        }
    }

    entangle_camera_manager_show(priv->manager);
    if (choose)
        entangle_camera_picker_show(priv->picker);

    return TRUE;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */