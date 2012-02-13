/*
 *  Entangle: Entangle Assists Photograph Aquisition
 *
 *  Copyright (C) 2009-2012 Daniel P. Berrange
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

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "entangle-debug.h"
#include "entangle-image.h"

#define ENTANGLE_IMAGE_GET_PRIVATE(obj)                                     \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), ENTANGLE_TYPE_IMAGE, EntangleImagePrivate))

struct _EntangleImagePrivate {
    char *filename;
    GdkPixbuf *pixbuf;
    GExiv2Metadata *metadata;

    gboolean dirty;
    struct stat st;
};

G_DEFINE_TYPE(EntangleImage, entangle_image, G_TYPE_OBJECT);

enum {
    PROP_0,
    PROP_FILENAME,
    PROP_PIXBUF,
    PROP_METADATA,
};

static void entangle_image_get_property(GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
    EntangleImage *picker = ENTANGLE_IMAGE(object);
    EntangleImagePrivate *priv = picker->priv;

    switch (prop_id)
        {
        case PROP_FILENAME:
            g_value_set_string(value, priv->filename);
            break;

        case PROP_PIXBUF:
            g_value_set_object(value, priv->pixbuf);
            break;

        case PROP_METADATA:
            g_value_set_object(value, priv->metadata);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}

static void entangle_image_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
    EntangleImage *picker = ENTANGLE_IMAGE(object);
    EntangleImagePrivate *priv = picker->priv;

    switch (prop_id)
        {
        case PROP_FILENAME:
            g_free(priv->filename);
            priv->filename = g_value_dup_string(value);
            priv->dirty = TRUE;
            break;

        case PROP_PIXBUF:
            if (priv->pixbuf)
                g_object_unref(priv->pixbuf);
            priv->pixbuf = g_value_get_object(value);
            if (priv->pixbuf)
                g_object_ref(priv->pixbuf);
            break;

        case PROP_METADATA:
            if (priv->metadata)
                g_object_unref(priv->metadata);
            priv->metadata = g_value_get_object(value);
            if (priv->metadata)
                g_object_ref(priv->metadata);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}


static void entangle_image_finalize(GObject *object)
{
    EntangleImage *image = ENTANGLE_IMAGE(object);
    EntangleImagePrivate *priv = image->priv;

    ENTANGLE_DEBUG("Finalize image %p", object);

    if (priv->pixbuf)
        g_object_unref(priv->pixbuf);
    if (priv->metadata)
        g_object_unref(priv->metadata);

    g_free(priv->filename);

    G_OBJECT_CLASS (entangle_image_parent_class)->finalize (object);
}


static void entangle_image_class_init(EntangleImageClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = entangle_image_finalize;
    object_class->get_property = entangle_image_get_property;
    object_class->set_property = entangle_image_set_property;

    g_object_class_install_property(object_class,
                                    PROP_FILENAME,
                                    g_param_spec_string("filename",
                                                        "Image filename",
                                                        "Full path to image file",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_object_class_install_property(object_class,
                                    PROP_PIXBUF,
                                    g_param_spec_object("pixbuf",
                                                        "Image pixbuf",
                                                        "Image pixbuf",
                                                        GDK_TYPE_PIXBUF,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
    g_object_class_install_property(object_class,
                                    PROP_METADATA,
                                    g_param_spec_object("metadata",
                                                        "Image metadata",
                                                        "Image metadata",
                                                        GEXIV2_TYPE_METADATA,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_type_class_add_private(klass, sizeof(EntangleImagePrivate));
}


EntangleImage *entangle_image_new_file(const char *filename)
{
    return ENTANGLE_IMAGE(g_object_new(ENTANGLE_TYPE_IMAGE,
                                       "filename", filename,
                                       NULL));
}


EntangleImage *entangle_image_new_pixbuf(GdkPixbuf *pixbuf)
{
    return ENTANGLE_IMAGE(g_object_new(ENTANGLE_TYPE_IMAGE,
                                       "pixbuf", pixbuf,
                                       NULL));
}


static void entangle_image_init(EntangleImage *picker)
{
    EntangleImagePrivate *priv;

    priv = picker->priv = ENTANGLE_IMAGE_GET_PRIVATE(picker);

    priv->dirty = TRUE;
}


const char *entangle_image_get_filename(EntangleImage *image)
{
    EntangleImagePrivate *priv = image->priv;
    return priv->filename;
}


static gboolean entangle_image_load(EntangleImage *image)
{
    EntangleImagePrivate *priv = image->priv;

    if (!priv->dirty)
        return TRUE;

    if (stat(priv->filename, &priv->st) < 0) {
        memset(&priv->st, 0, sizeof priv->st);
        return FALSE;
    }

    priv->dirty = FALSE;
    return TRUE;
}

time_t entangle_image_get_last_modified(EntangleImage *image)
{
    EntangleImagePrivate *priv = image->priv;

    if (!entangle_image_load(image))
        return 0;

    return priv->st.st_mtime;
}

off_t entangle_image_get_file_size(EntangleImage *image)
{
    EntangleImagePrivate *priv = image->priv;

    if (!entangle_image_load(image))
        return 0;

    return priv->st.st_size;
}


GdkPixbuf *entangle_image_get_pixbuf(EntangleImage *image)
{
    EntangleImagePrivate *priv = image->priv;
    return priv->pixbuf;
}

void entangle_image_set_pixbuf(EntangleImage *image,
                               GdkPixbuf *pixbuf)
{
    g_object_set(image, "pixbuf", pixbuf, NULL);
}


GExiv2Metadata *entangle_image_get_metadata(EntangleImage *image)
{
    EntangleImagePrivate *priv = image->priv;
    return priv->metadata;
}

void entangle_image_set_metadata(EntangleImage *image,
                                 GExiv2Metadata *metadata)
{
    g_object_set(image, "metadata", metadata, NULL);
}



/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
