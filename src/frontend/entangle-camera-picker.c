/*
 *  Entangle: Tethered Camera Control & Capture
 *
 *  Copyright (C) 2009-2015 Daniel P. Berrange
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

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <unistd.h>

#include "entangle-debug.h"
#include "entangle-camera-picker.h"
#include "entangle-window.h"

#define ENTANGLE_CAMERA_PICKER_GET_PRIVATE(obj)                         \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), ENTANGLE_TYPE_CAMERA_PICKER, EntangleCameraPickerPrivate))

gboolean do_picker_close(GtkButton *src,
                         gpointer data);
gboolean do_picker_delete(GtkWidget *src,
                          GdkEvent *ev);

struct _EntangleCameraPickerPrivate {
    EntangleCameraList *cameras;
    gulong addSignal;
    gulong removeSignal;

    GtkListStore *model;

    GtkBuilder *builder;
};

static void entangle_camera_picker_window_interface_init(gpointer g_iface,
                                                         gpointer iface_data);

G_DEFINE_TYPE_EXTENDED(EntangleCameraPicker, entangle_camera_picker, GTK_TYPE_DIALOG, 0,
                       G_IMPLEMENT_INTERFACE(ENTANGLE_TYPE_WINDOW, entangle_camera_picker_window_interface_init));

enum {
    PROP_O,
    PROP_CAMERAS
};

void do_picker_refresh(GtkButton *src,
                       EntangleCameraPicker *picker);
void do_picker_activate(GtkTreeView *src,
                        GtkTreePath *path,
                        GtkTreeViewColumn *col,
                        EntangleCameraPicker *picker);
void do_picker_connect(GtkButton *src,
                       EntangleCameraPicker *picker);


static void entangle_camera_cell_data_model_func(GtkTreeViewColumn *col G_GNUC_UNUSED,
                                                 GtkCellRenderer *cell,
                                                 GtkTreeModel *model,
                                                 GtkTreeIter *iter,
                                                 gpointer data G_GNUC_UNUSED)
{
    GValue val;
    EntangleCamera *cam;

    memset(&val, 0, sizeof val);

    gtk_tree_model_get_value(model, iter, 0, &val);

    cam = g_value_get_object(&val);

    g_object_set(cell, "text", entangle_camera_get_model(cam), NULL);

    g_object_unref(cam);
}


static void entangle_camera_cell_data_port_func(GtkTreeViewColumn *col G_GNUC_UNUSED,
                                                GtkCellRenderer *cell,
                                                GtkTreeModel *model,
                                                GtkTreeIter *iter,
                                                gpointer data G_GNUC_UNUSED)
{
    GValue val;
    EntangleCamera *cam;

    memset(&val, 0, sizeof val);

    gtk_tree_model_get_value(model, iter, 0, &val);

    cam = g_value_get_object(&val);

    g_object_set(cell, "text", entangle_camera_get_port(cam), NULL);

    g_object_unref(cam);
}


static void entangle_camera_cell_data_capture_func(GtkTreeViewColumn *col G_GNUC_UNUSED,
                                                   GtkCellRenderer *cell,
                                                   GtkTreeModel *model,
                                                   GtkTreeIter *iter,
                                                   gpointer data G_GNUC_UNUSED)
{
    GValue val;
    EntangleCamera *cam;

    memset(&val, 0, sizeof val);

    gtk_tree_model_get_value(model, iter, 0, &val);

    cam = g_value_get_object(&val);

    ENTANGLE_DEBUG("Has %d", entangle_camera_get_has_capture(cam));

    g_object_set(cell, "text", entangle_camera_get_has_capture(cam) ? _("Yes") : _("No"), NULL);

    g_object_unref(cam);
}


static void do_model_sensitivity_update(EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    EntangleCameraPickerPrivate *priv = picker->priv;
    GtkWidget *warning;
    GtkWidget *list;
    GtkWidget *win;

    warning = GTK_WIDGET(gtk_builder_get_object(priv->builder, "warning-no-cameras"));
    list = GTK_WIDGET(gtk_builder_get_object(priv->builder, "camera-list"));
    win = GTK_WIDGET(gtk_builder_get_object(priv->builder, "camera-picker"));

    if (priv->cameras && entangle_camera_list_count(priv->cameras)) {
        int w, h;
        gtk_window_get_default_size(GTK_WINDOW(win), &w, &h);
        gtk_window_resize(GTK_WINDOW(win), w, h);
        gtk_widget_set_sensitive(list, TRUE);
        gtk_widget_hide(warning);
    } else {
        gtk_widget_set_sensitive(list, FALSE);
        gtk_widget_show(warning);
    }
}


static void do_model_refresh(EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    EntangleCameraPickerPrivate *priv = picker->priv;
    ENTANGLE_DEBUG("Refresh model");
    gtk_list_store_clear(priv->model);

    if (!priv->cameras) {
        do_model_sensitivity_update(picker);
        return;
    }

    for (int i = 0; i < entangle_camera_list_count(priv->cameras); i++) {
        EntangleCamera *cam = entangle_camera_list_get(priv->cameras, i);
        GtkTreeIter iter;

        gtk_list_store_append(priv->model, &iter);

        gtk_list_store_set(priv->model, &iter, 0, cam, -1);

        //g_object_unref(cam);
    }

    do_model_sensitivity_update(picker);
}


static void entangle_camera_picker_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec)
{
    EntangleCameraPicker *picker = ENTANGLE_CAMERA_PICKER(object);
    EntangleCameraPickerPrivate *priv = picker->priv;

    switch (prop_id)
        {
        case PROP_CAMERAS:
            g_value_set_object(value, priv->cameras);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}


static void do_camera_list_add(EntangleCameraList *cameras G_GNUC_UNUSED,
                               EntangleCamera *cam,
                               EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    EntangleCameraPickerPrivate *priv = picker->priv;
    GtkTreeIter iter;

    ENTANGLE_DEBUG("Add camrea %p to model", cam);
    gtk_list_store_append(priv->model, &iter);

    gtk_list_store_set(priv->model, &iter, 0, cam, -1);

    do_model_sensitivity_update(picker);
}


static void do_camera_list_remove(EntangleCameraList *cameras G_GNUC_UNUSED,
                                  EntangleCamera *cam,
                                  EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    EntangleCameraPickerPrivate *priv = picker->priv;
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->model), &iter))
        return;

    do {
        GValue val;
        EntangleCamera *thiscam;
        memset(&val, 0, sizeof val);

        gtk_tree_model_get_value(GTK_TREE_MODEL(priv->model), &iter, 0, &val);

        thiscam = g_value_get_object(&val);
        g_value_unset(&val);

        if (cam == thiscam) {
            ENTANGLE_DEBUG("Remove camera %p from model", cam);
            gtk_list_store_remove(priv->model, &iter);
            break;
        }

    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->model), &iter));

    do_model_sensitivity_update(picker);
}


static void entangle_camera_picker_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec)
{
    EntangleCameraPicker *picker = ENTANGLE_CAMERA_PICKER(object);

    ENTANGLE_DEBUG("Set prop %d", prop_id);

    switch (prop_id)
        {
        case PROP_CAMERAS:
            entangle_camera_picker_set_camera_list(picker,
                                                   ENTANGLE_CAMERA_LIST(g_value_get_object(value)));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}


static void entangle_camera_picker_finalize(GObject *object)
{
    EntangleCameraPicker *picker = ENTANGLE_CAMERA_PICKER(object);
    EntangleCameraPickerPrivate *priv = picker->priv;

    ENTANGLE_DEBUG("Finalize camera picker");

    gtk_list_store_clear(priv->model);
    if (priv->cameras)
        g_object_unref(priv->cameras);
    g_object_unref(priv->model);
    g_object_unref(priv->builder);

    G_OBJECT_CLASS(entangle_camera_picker_parent_class)->finalize(object);
}

static void do_entangle_camera_picker_set_builder(EntangleWindow *window,
                                                  GtkBuilder *builder);
static GtkBuilder *do_entangle_camera_picker_get_builder(EntangleWindow *window);


static void entangle_camera_picker_window_interface_init(gpointer g_iface,
                                                         gpointer iface_data G_GNUC_UNUSED)
{
    EntangleWindowInterface *iface = g_iface;
    iface->set_builder = do_entangle_camera_picker_set_builder;
    iface->get_builder = do_entangle_camera_picker_get_builder;
}


static void entangle_camera_picker_class_init(EntangleCameraPickerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_camera_picker_finalize;
    object_class->get_property = entangle_camera_picker_get_property;
    object_class->set_property = entangle_camera_picker_set_property;

    g_signal_new("picker-connect",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(EntangleCameraPickerClass, picker_connect),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE,
                 1,
                 G_TYPE_OBJECT);
    g_signal_new("picker-refresh",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(EntangleCameraPickerClass, picker_refresh),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);

    g_object_class_install_property(object_class,
                                    PROP_CAMERAS,
                                    g_param_spec_object("cameras",
                                                        "Camera List",
                                                        "List of known camera objects",
                                                        ENTANGLE_TYPE_CAMERA_LIST,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_type_class_add_private(klass, sizeof(EntangleCameraPickerPrivate));
}


EntangleCameraPicker *entangle_camera_picker_new(void)
{
    return ENTANGLE_CAMERA_PICKER(entangle_window_new(ENTANGLE_TYPE_CAMERA_PICKER,
                                                      GTK_TYPE_DIALOG,
                                                      "camera-picker"));
}


gboolean do_picker_close(GtkButton *src G_GNUC_UNUSED,
                         gpointer data)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_PICKER(data), FALSE);

    EntangleCameraPicker *picker = ENTANGLE_CAMERA_PICKER(data);

    ENTANGLE_DEBUG("picker delete");
    gtk_widget_hide(GTK_WIDGET(picker));
    return TRUE;
}

gboolean do_picker_delete(GtkWidget *src,
                          GdkEvent *ev G_GNUC_UNUSED)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_PICKER(src), FALSE);

    ENTANGLE_DEBUG("picker delete");
    gtk_widget_hide(src);
    return TRUE;
}

void do_picker_refresh(GtkButton *src G_GNUC_UNUSED,
                       EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    ENTANGLE_DEBUG("picker refresh %p", picker);
    g_signal_emit_by_name(picker, "picker-refresh", NULL);
}

static EntangleCamera *entangle_picker_get_selected_camera(EntangleCameraPicker *picker)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker), NULL);

    EntangleCameraPickerPrivate *priv = picker->priv;
    GtkWidget *list;
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    gboolean selected;
    GValue val;

    ENTANGLE_DEBUG("select camera");

    list = GTK_WIDGET(gtk_builder_get_object(priv->builder, "camera-list"));

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));

    selected = gtk_tree_selection_get_selected(sel, NULL, &iter);
    if (!selected)
        return NULL;

    memset(&val, 0, sizeof val);
    gtk_tree_model_get_value(GTK_TREE_MODEL(priv->model), &iter, 0, &val);

    return g_value_get_object(&val);
}


void do_picker_activate(GtkTreeView *src G_GNUC_UNUSED,
                        GtkTreePath *path G_GNUC_UNUSED,
                        GtkTreeViewColumn *col G_GNUC_UNUSED,
                        EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    EntangleCamera *cam;
    cam = entangle_picker_get_selected_camera(picker);
    ENTANGLE_DEBUG("picker activate %p %p", picker, cam);

    if (cam) {
        GValue val;
        memset(&val, 0, sizeof val);
        g_value_init(&val, G_TYPE_OBJECT);
        g_value_set_object(&val, cam);
        //g_signal_emit_by_name(picker, "picker-connect", &val);
        g_signal_emit_by_name(picker, "picker-connect", cam);
        g_value_unset(&val);
        g_object_unref(cam);
    }
}


void do_picker_connect(GtkButton *src G_GNUC_UNUSED,
                       EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    EntangleCamera *cam;
    cam = entangle_picker_get_selected_camera(picker);
    ENTANGLE_DEBUG("picker connect %p %p", picker, cam);
    if (cam) {
        GValue val;
        memset(&val, 0, sizeof val);
        g_value_init(&val, G_TYPE_OBJECT);
        g_value_set_object(&val, cam);
        //g_signal_emit_by_name(picker, "picker-connect", &val);
        g_signal_emit_by_name(picker, "picker-connect", cam);
        g_value_unset(&val);
        g_object_unref(cam);
    }
}


static void do_camera_select(GtkTreeSelection *sel, EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    EntangleCameraPickerPrivate *priv = picker->priv;
    GtkWidget *connect;
    GtkTreeIter iter;
    gboolean selected;

    ENTANGLE_DEBUG("selection changed");

    connect = GTK_WIDGET(gtk_builder_get_object(priv->builder, "picker-connect"));

    selected = gtk_tree_selection_get_selected(sel, NULL, &iter);
    gtk_widget_set_sensitive(connect, selected);
}


static void do_entangle_camera_picker_set_builder(EntangleWindow *win,
                                                  GtkBuilder *builder)
{
    EntangleCameraPicker *picker = ENTANGLE_CAMERA_PICKER(win);
    EntangleCameraPickerPrivate *priv = picker->priv;
    GtkWidget *list;
    GtkCellRenderer *model;
    GtkCellRenderer *port;
    GtkCellRenderer *capture;
    GtkTreeViewColumn *modelCol;
    GtkTreeViewColumn *portCol;
    GtkTreeViewColumn *captureCol;
    GtkTreeSelection *sel;
    GtkWidget *connect;

    priv->builder = g_object_ref(builder);

    list = GTK_WIDGET(gtk_builder_get_object(priv->builder, "camera-list"));

    model = gtk_cell_renderer_text_new();
    port = gtk_cell_renderer_text_new();
    capture = gtk_cell_renderer_text_new();

    modelCol = gtk_tree_view_column_new_with_attributes(_("Model"), model, NULL);
    portCol = gtk_tree_view_column_new_with_attributes(_("Port"), port, NULL);
    captureCol = gtk_tree_view_column_new_with_attributes(_("Capture"), capture, NULL);

    g_object_set(modelCol, "expand", TRUE, NULL);
    g_object_set(portCol, "expand", FALSE, NULL);
    g_object_set(captureCol, "expand", FALSE, NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(list), modelCol);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), portCol);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), captureCol);

    gtk_tree_view_column_set_cell_data_func(modelCol, model, entangle_camera_cell_data_model_func, NULL, NULL);
    gtk_tree_view_column_set_cell_data_func(portCol, port, entangle_camera_cell_data_port_func, NULL, NULL);
    gtk_tree_view_column_set_cell_data_func(captureCol, capture, entangle_camera_cell_data_capture_func, NULL, NULL);

    gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(priv->model));

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));

    g_signal_connect(sel, "changed", G_CALLBACK(do_camera_select), picker);

    connect = GTK_WIDGET(gtk_builder_get_object(priv->builder, "picker-connect"));
    gtk_widget_set_sensitive(connect, FALSE);

    do_model_sensitivity_update(picker);
}


static GtkBuilder *do_entangle_camera_picker_get_builder(EntangleWindow *window)
{
    EntangleCameraPicker *picker = ENTANGLE_CAMERA_PICKER(window);
    EntangleCameraPickerPrivate *priv = picker->priv;

    return priv->builder;
}


static void entangle_camera_picker_init(EntangleCameraPicker *picker)
{
    EntangleCameraPickerPrivate *priv;

    priv = picker->priv = ENTANGLE_CAMERA_PICKER_GET_PRIVATE(picker);

    priv->model = gtk_list_store_new(1, G_TYPE_OBJECT);
}


void entangle_camera_picker_set_camera_list(EntangleCameraPicker *picker,
                                            EntangleCameraList *cameras)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    EntangleCameraPickerPrivate *priv = picker->priv;

    if (priv->cameras) {
        g_signal_handler_disconnect(priv->cameras, priv->addSignal);
        g_signal_handler_disconnect(priv->cameras, priv->removeSignal);
        g_object_unref(priv->cameras);
    }
    priv->cameras = g_object_ref(cameras);
    priv->addSignal = g_signal_connect(priv->cameras, "camera-added",
                                       G_CALLBACK(do_camera_list_add), picker);
    priv->removeSignal = g_signal_connect(priv->cameras, "camera-removed",
                                          G_CALLBACK(do_camera_list_remove), picker);
    do_model_refresh(picker);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
