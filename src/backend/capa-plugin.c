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

#include <config.h>

#include <string.h>

#include "capa-debug.h"
#include "capa-plugin.h"

#define CAPA_PLUGIN_GET_PRIVATE(obj)                                    \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), CAPA_TYPE_PLUGIN, CapaPluginPrivate))

struct _CapaPluginPrivate {
    gboolean active;
    gchar *dir;
    gchar *name;
    gchar *description;
    gchar *version;
    gchar *uri;
    gchar *email;
};

enum {
    PROP_0,
    PROP_DIR,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_VERSION,
    PROP_URI,
    PROP_EMAIL,
};

G_DEFINE_ABSTRACT_TYPE(CapaPlugin, capa_plugin, G_TYPE_OBJECT);


static void capa_plugin_get_property(GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    CapaPlugin *plugin = CAPA_PLUGIN(object);
    CapaPluginPrivate *priv = plugin->priv;

    switch (prop_id)
        {
        case PROP_DIR:
            g_value_set_string(value, priv->dir);
            break;

        case PROP_NAME:
            g_value_set_string(value, priv->name);
            break;

        case PROP_DESCRIPTION:
            g_value_set_string(value, priv->description);
            break;

        case PROP_VERSION:
            g_value_set_string(value, priv->version);
            break;

        case PROP_URI:
            g_value_set_string(value, priv->uri);
            break;

        case PROP_EMAIL:
            g_value_set_string(value, priv->email);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}


static void capa_plugin_set_property(GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    CapaPlugin *plugin = CAPA_PLUGIN(object);
    CapaPluginPrivate *priv = plugin->priv;

    CAPA_DEBUG("Set prop %d", prop_id);

    switch (prop_id)
        {
        case PROP_DIR:
            g_free(priv->dir);
            priv->dir = g_value_dup_string(value);
            break;

        case PROP_NAME:
            g_free(priv->name);
            priv->name = g_value_dup_string(value);
            break;

        case PROP_DESCRIPTION:
            g_free(priv->description);
            priv->description = g_value_dup_string(value);
            break;

        case PROP_VERSION:
            g_free(priv->version);
            priv->version = g_value_dup_string(value);
            break;

        case PROP_URI:
            g_free(priv->uri);
            priv->uri = g_value_dup_string(value);
            break;

        case PROP_EMAIL:
            g_free(priv->email);
            priv->email = g_value_dup_string(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}



static void capa_plugin_finalize(GObject *object)
{
    CapaPlugin *plugin = CAPA_PLUGIN(object);
    CapaPluginPrivate *priv = plugin->priv;

    g_free(priv->name);
    g_free(priv->description);
    g_free(priv->version);
    g_free(priv->uri);
    g_free(priv->email);

    G_OBJECT_CLASS(capa_plugin_parent_class)->finalize (object);
}

static void capa_plugin_class_init(CapaPluginClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = capa_plugin_finalize;
    object_class->get_property = capa_plugin_get_property;
    object_class->set_property = capa_plugin_set_property;

    g_object_class_install_property(object_class,
                                    PROP_DIR,
                                    g_param_spec_string("dir",
                                                        "Dir",
                                                        "Plugin dir",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_object_class_install_property(object_class,
                                    PROP_NAME,
                                    g_param_spec_string("name",
                                                        "Name",
                                                        "Plugin name",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_object_class_install_property(object_class,
                                    PROP_DESCRIPTION,
                                    g_param_spec_string("description",
                                                        "Description",
                                                        "Plugin description",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_object_class_install_property(object_class,
                                    PROP_VERSION,
                                    g_param_spec_string("version",
                                                        "Version",
                                                        "Plugin version",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_object_class_install_property(object_class,
                                    PROP_EMAIL,
                                    g_param_spec_string("email",
                                                        "Email",
                                                        "Plugin email",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_object_class_install_property(object_class,
                                    PROP_URI,
                                    g_param_spec_string("uri",
                                                        "Uri",
                                                        "Plugin uri",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_type_class_add_private(klass, sizeof(CapaPluginPrivate));
}


void capa_plugin_init(CapaPlugin *plugin)
{
    CapaPluginPrivate *priv;

    priv = plugin->priv = CAPA_PLUGIN_GET_PRIVATE(plugin);
    memset(priv, 0, sizeof(*priv));
}


gboolean capa_plugin_activate(CapaPlugin *plugin, GObject *app)
{
    return CAPA_PLUGIN_GET_CLASS(plugin)->activate(plugin, app);
}

gboolean capa_plugin_deactivate(CapaPlugin *plugin, GObject *app)
{
    return CAPA_PLUGIN_GET_CLASS(plugin)->deactivate(plugin, app);
}

gboolean capa_plugin_is_active(CapaPlugin *plugin)
{
    return CAPA_PLUGIN_GET_CLASS(plugin)->is_active(plugin);
}

const gchar *
capa_plugin_get_dir(CapaPlugin *plugin)
{
    CapaPluginPrivate *priv = plugin->priv;
    return priv->dir;
}

const gchar *
capa_plugin_get_name(CapaPlugin *plugin)
{
    CapaPluginPrivate *priv = plugin->priv;
    return priv->name;
}

const gchar *
capa_plugin_get_description(CapaPlugin *plugin)
{
    CapaPluginPrivate *priv = plugin->priv;
    return priv->description;
}

const gchar *
capa_plugin_get_version(CapaPlugin *plugin)
{
    CapaPluginPrivate *priv = plugin->priv;
    return priv->version;
}

const gchar *
capa_plugin_get_uri(CapaPlugin *plugin)
{
    CapaPluginPrivate *priv = plugin->priv;
    return priv->uri;
}

const gchar *
capa_plugin_get_email(CapaPlugin *plugin)
{
    CapaPluginPrivate *priv = plugin->priv;
    return priv->email;
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
