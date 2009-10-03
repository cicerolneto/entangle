/*
 *  Capa: Capa Assists Photograph Aquisition
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

#include <string.h>

#include "control-panel.h"
#include "control.h"

#define CAPA_CONTROL_PANEL_GET_PRIVATE(obj) \
      (G_TYPE_INSTANCE_GET_PRIVATE((obj), CAPA_TYPE_CONTROL_PANEL, CapaControlPanelPrivate))

struct _CapaControlPanelPrivate {
  CapaCamera *camera;
};

G_DEFINE_TYPE(CapaControlPanel, capa_control_panel, GTK_TYPE_VBOX);

enum {
  PROP_O,
  PROP_CAMERA,
};


static void do_control_remove(GtkWidget *widget,
			      gpointer data)
{
  CapaControlPanel *panel = data;

  gtk_container_remove(GTK_CONTAINER(panel), widget);
}

static void do_setup_control_group(CapaControlPanel *panel,
				   GtkVBox *box,
				   CapaControlGroup *grp)
{
  int i;

  for (i = 0 ; i < capa_control_group_count(grp) ; i++) {
    CapaControl *control = capa_control_group_get(grp, i);

    if (CAPA_IS_CONTROL_GROUP(control)) {
      GtkWidget *frame = gtk_expander_new(capa_control_label(control));
      //GtkWidget *frame = gtk_frame_new(capa_control_label(control));
      GtkWidget *subbox = gtk_vbox_new(FALSE, 6);

      gtk_container_add(GTK_CONTAINER(box), frame);
      //gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
      gtk_expander_set_expanded(GTK_EXPANDER(frame), TRUE);
      gtk_container_set_border_width(GTK_CONTAINER(subbox), 6);
      //g_object_unref(G_OBJECT(frame));

      gtk_container_add(GTK_CONTAINER(frame), subbox);
      //g_object_unref(G_OBJECT(subbox));

      do_setup_control_group(panel, GTK_VBOX(subbox), CAPA_CONTROL_GROUP(control));
    } else {
      //GtkWidget *subbox = gtk_hbox_new(FALSE, 6);
      GtkWidget *label = gtk_label_new(capa_control_label(control));
      GtkWidget *entry = gtk_entry_new();

      gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
      gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
      gtk_widget_set_tooltip_text(label, capa_control_info(control));

      //gtk_container_add(GTK_CONTAINER(box), subbox);
      //g_object_unref(G_OBJECT(subbox));

      //gtk_container_add(GTK_CONTAINER(subbox), label);
      //gtk_container_add(GTK_CONTAINER(subbox), entry);
      gtk_container_add(GTK_CONTAINER(box), label);
      gtk_container_add(GTK_CONTAINER(box), entry);
    }
  }
}

static void do_setup_camera(CapaControlPanel *panel)
{
  CapaControlPanelPrivate *priv = panel->priv;
  CapaControlGroup *grp;

  gtk_container_foreach(GTK_CONTAINER(panel), do_control_remove, panel);

  if (!priv->camera)
    return;

  grp = capa_camera_controls(priv->camera);

  do_setup_control_group(panel, GTK_VBOX(panel), grp);
  gtk_widget_show_all(GTK_WIDGET(panel));

  g_object_unref(G_OBJECT(grp));
}

static void capa_control_panel_get_property(GObject *object,
					    guint prop_id,
					    GValue *value,
					    GParamSpec *pspec)
{
  CapaControlPanel *panel = CAPA_CONTROL_PANEL(object);
  CapaControlPanelPrivate *priv = panel->priv;

  switch (prop_id)
    {
    case PROP_CAMERA:
      g_value_set_object(value, priv->camera);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void capa_control_panel_set_property(GObject *object,
					    guint prop_id,
					    const GValue *value,
					    GParamSpec *pspec)
{
  CapaControlPanel *panel = CAPA_CONTROL_PANEL(object);
  CapaControlPanelPrivate *priv = panel->priv;

  fprintf(stderr, "Set prop on control panel %d\n", prop_id);

  switch (prop_id)
    {
    case PROP_CAMERA:
      if (priv->camera)
	g_object_unref(G_OBJECT(priv->camera));
      priv->camera = g_value_get_object(value);
      g_object_ref(G_OBJECT(priv->camera));
      do_setup_camera(panel);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void capa_control_panel_finalize (GObject *object)
{
  CapaControlPanel *panel = CAPA_CONTROL_PANEL(object);
  CapaControlPanelPrivate *priv = panel->priv;

  if (priv->camera)
    g_object_unref(G_OBJECT(priv->camera));

  G_OBJECT_CLASS (capa_control_panel_parent_class)->finalize (object);
}


static void capa_control_panel_class_init(CapaControlPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->finalize = capa_control_panel_finalize;
  object_class->get_property = capa_control_panel_get_property;
  object_class->set_property = capa_control_panel_set_property;

  g_object_class_install_property(object_class,
				  PROP_CAMERA,
				  g_param_spec_object("camera",
						      "Camera",
						      "Camera to managed",
						      CAPA_TYPE_CAMERA,
						      G_PARAM_READWRITE |
						      G_PARAM_STATIC_NAME |
						      G_PARAM_STATIC_NICK |
						      G_PARAM_STATIC_BLURB));

  g_type_class_add_private(klass, sizeof(CapaControlPanelPrivate));
}

CapaControlPanel *capa_control_panel_new(void)
{
  return CAPA_CONTROL_PANEL(g_object_new(CAPA_TYPE_CONTROL_PANEL, NULL));
}


static void capa_control_panel_init(CapaControlPanel *panel)
{
  CapaControlPanelPrivate *priv;

  priv = panel->priv = CAPA_CONTROL_PANEL_GET_PRIVATE(panel);
  memset(priv, 0, sizeof *priv);

  //gtk_container_set_border_width(GTK_CONTAINER(panel), 6);
}
