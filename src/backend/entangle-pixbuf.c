/*
 *  Entangle: Tethered Camera Control & Capture
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

#include <libraw/libraw.h>

#include "entangle-debug.h"
#include "entangle-pixbuf.h"

/* Performs rotation of the thunbnail based on embedded metadata */
GdkPixbuf *entangle_pixbuf_auto_rotate(GdkPixbuf *src)
{
    GdkPixbuf *dest = gdk_pixbuf_apply_embedded_orientation(src);
    GdkPixbuf *temp;

    ENTANGLE_DEBUG("Auto-rotate %p %p\n", src, dest);

    if (dest == src) {
        const char *orientationstr;
        int transform = 0;
        orientationstr = gdk_pixbuf_get_option(src, "tEXt::Entangle::Orientation");

        /* If not option, then try the gobject data slot */
        if (!orientationstr)
            orientationstr = g_object_get_data(G_OBJECT(src),
                                               "tEXt::Entangle::Orientation");

        if (orientationstr)
            transform = (int)g_ascii_strtoll(orientationstr, NULL, 10);

	ENTANGLE_DEBUG("Auto-rotate %s\n", orientationstr);

        /* Apply the actual transforms, which involve rotations and flips.
           The meaning of orientation values 1-8 and the required transforms
           are defined by the TIFF and EXIF (for JPEGs) standards. */
        switch (transform) {
        case GEXIV2_ORIENTATION_NORMAL:
            dest = src;
            g_object_ref(dest);
            break;
        case GEXIV2_ORIENTATION_HFLIP:
            dest = gdk_pixbuf_flip(src, TRUE);
            break;
        case GEXIV2_ORIENTATION_ROT_180:
            dest = gdk_pixbuf_rotate_simple(src, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
            break;
        case GEXIV2_ORIENTATION_VFLIP:
            dest = gdk_pixbuf_flip(src, FALSE);
            break;
        case GEXIV2_ORIENTATION_ROT_90_HFLIP:
            temp = gdk_pixbuf_rotate_simple(src, GDK_PIXBUF_ROTATE_CLOCKWISE);
            dest = gdk_pixbuf_flip(temp,TRUE);
            g_object_unref(temp);
            break;
        case GEXIV2_ORIENTATION_ROT_90:
            dest = gdk_pixbuf_rotate_simple(src, GDK_PIXBUF_ROTATE_CLOCKWISE);
            break;
        case GEXIV2_ORIENTATION_ROT_90_VFLIP:
            temp = gdk_pixbuf_rotate_simple(src, GDK_PIXBUF_ROTATE_CLOCKWISE);
            dest = gdk_pixbuf_flip(temp, FALSE);
            g_object_unref(temp);
            break;
        case GEXIV2_ORIENTATION_ROT_270:
            dest = gdk_pixbuf_rotate_simple(src, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
            break;
        default:
            /* if no orientation tag was present */
            dest = src;
            g_object_ref(dest);
            break;
        }

    }

    g_object_unref(src);

    return dest;
}

static gboolean entangle_pixbuf_is_raw(EntangleImage *image)
{
    const char *extlist[] = {
        ".cr2", ".nef", ".nrw", ".arw", ".orf", ".dng", ".pef",
        ".crw", ".erf", ".mrw", ".raw", ".rw2", ".raf", NULL
    };
    const char **tmp;
    const char *filename = entangle_image_get_filename(image);

    tmp = extlist;
    while (*tmp) {
        const char *ext = *tmp;

        if (g_str_has_suffix(filename, ext))
            return TRUE;

        tmp++;
    }

    return FALSE;
}


static void img_free(guchar *ignore G_GNUC_UNUSED, gpointer opaque)
{
    libraw_processed_image_t *img = opaque;
    libraw_dcraw_clear_mem(img);
}


static GdkPixbuf *entangle_pixbuf_open_image_master_raw(EntangleImage *image)
{
    GdkPixbuf *result = NULL;
    libraw_data_t *raw = libraw_init(0);
    libraw_processed_image_t *img = NULL;
    int ret;

    if (!raw) {
        ENTANGLE_DEBUG("Failed to initialize libraw");
        goto cleanup;
    }

    ENTANGLE_DEBUG("Open raw %s", entangle_image_get_filename(image));
    if ((ret = libraw_open_file(raw, entangle_image_get_filename(image))) != 0) {
        ENTANGLE_DEBUG("Failed to open raw file: %s",
                       libraw_strerror(ret));
        goto cleanup;
    }

    ENTANGLE_DEBUG("Unpack raw %s", entangle_image_get_filename(image));
    if ((ret = libraw_unpack(raw)) != 0) {
        ENTANGLE_DEBUG("Failed to unpack raw file: %s",
                       libraw_strerror(ret));
        goto cleanup;
    }

    ENTANGLE_DEBUG("Process raw %s", entangle_image_get_filename(image));
    if ((ret = libraw_dcraw_process(raw)) != 0) {
        ENTANGLE_DEBUG("Failed to process raw file: %s",
                       libraw_strerror(ret));
        goto cleanup;
    }

    ENTANGLE_DEBUG("Make mem %s", entangle_image_get_filename(image));
    if ((img = libraw_dcraw_make_mem_image(raw, &ret)) == NULL) {
        ENTANGLE_DEBUG("Failed to extract raw file: %s",
                       libraw_strerror(ret));
        goto cleanup;
    }

    ENTANGLE_DEBUG("New pixbuf %s", entangle_image_get_filename(image));
    result = gdk_pixbuf_new_from_data(img->data,
                                      GDK_COLORSPACE_RGB,
                                      FALSE, img->bits,
                                      img->width, img->height,
                                      (img->width * img->bits * 3)/8,
                                      img_free, img);

 cleanup:
    libraw_close(raw);
    return result;
}


static GdkPixbuf *entangle_pixbuf_open_image_master_gdk(EntangleImage *image)
{
    GdkPixbuf *master;
    GdkPixbuf *result;

    ENTANGLE_DEBUG("Loading %s using GDK Pixbuf", entangle_image_get_filename(image));
    master = gdk_pixbuf_new_from_file(entangle_image_get_filename(image), NULL);

    if (!master)
        return NULL;

    result = gdk_pixbuf_apply_embedded_orientation(master);

    g_object_unref(master);
    return result;
}


static GdkPixbuf *entangle_pixbuf_open_image_master(EntangleImage *image)
{
    if (entangle_pixbuf_is_raw(image))
        return entangle_pixbuf_open_image_master_raw(image);
    else
        return entangle_pixbuf_open_image_master_gdk(image);
}


static GExiv2PreviewProperties *
entangle_pixbuf_get_largest_preview(GExiv2PreviewProperties **proplist)
{
    GExiv2PreviewProperties *best = NULL;
    gint bestw = 0, besth = 0;

    while (proplist && *proplist) {
        gint w, h;
        w = gexiv2_preview_properties_get_width(*proplist);
        h = gexiv2_preview_properties_get_height(*proplist);
        if (!best || ((w > bestw) && (h > besth))) {
            best = *proplist;
            bestw = w;
            besth = h;
        }
        proplist++;
    }
    ENTANGLE_DEBUG("Largest preview %p %dx%d", best, bestw, besth);
    return best;
}


static GExiv2PreviewProperties *
entangle_pixbuf_get_closest_preview(GExiv2PreviewProperties **proplist,
                                    guint minSize)
{
    GExiv2PreviewProperties *best = NULL;
    gint bestw = 0, besth = 0;

    while (proplist && *proplist) {
        gint w, h;
        w = gexiv2_preview_properties_get_width(*proplist);
        h = gexiv2_preview_properties_get_height(*proplist);
        ENTANGLE_DEBUG("Check %dx%d vs %d (best %p %dx%d",
                       w, h, minSize, best, bestw, besth);
        if (w > minSize && h > minSize) {
            if (!best ||
                ((w < bestw) && (h < besth))) {
                best = *proplist;
                bestw = w;
                besth = h;
            }
        }
        proplist++;
    }
    ENTANGLE_DEBUG("Closest preview %p %dx%d", best, bestw, besth);
    return best;
}


static GdkPixbuf *entangle_pixbuf_open_image_preview_exiv(EntangleImage *image,
							  guint minSize)
{
    GExiv2Metadata *metadata = gexiv2_metadata_new();
    GExiv2PreviewImage *preview = NULL;
    GExiv2PreviewProperties **props;
    GExiv2PreviewProperties *best;
    GdkPixbuf *result = NULL, *master = NULL;
    GdkPixbufLoader *loader = NULL;
    const guint8 *data;
    guint32 datalen;
    GExiv2Orientation orient;

    ENTANGLE_DEBUG("Opening preview %s", entangle_image_get_filename(image));

    if (!gexiv2_metadata_open_path(metadata, entangle_image_get_filename(image), NULL)) {
        ENTANGLE_DEBUG("Unable to load exiv2 data");
        goto cleanup;
    }

    props = gexiv2_metadata_get_preview_properties(metadata);

    if (minSize)
        best = entangle_pixbuf_get_closest_preview(props, minSize);
    else
        best = entangle_pixbuf_get_largest_preview(props);
    if (!best) {
        ENTANGLE_DEBUG("No preview properties for %s", entangle_image_get_filename(image));
        goto cleanup;
    }

    preview = gexiv2_metadata_get_preview_image(metadata, best);

    loader = gdk_pixbuf_loader_new_with_mime_type(gexiv2_preview_image_get_mime_type(preview),
                                                  NULL);

    data = gexiv2_preview_image_get_data(preview, &datalen);
    gdk_pixbuf_loader_write(loader,
                            data, datalen,
                            NULL);

    if (!gdk_pixbuf_loader_close(loader, NULL)) {
        ENTANGLE_DEBUG("Failed to load preview image for %s", entangle_image_get_filename(image));
        goto cleanup;
    }

    if (!(master = gdk_pixbuf_loader_get_pixbuf(loader))) {
        ENTANGLE_DEBUG("Failed to parse preview for %s", entangle_image_get_filename(image));
        goto cleanup;
    }

    orient = gexiv2_metadata_get_orientation(metadata);
    /* gdk_pixbuf_save doesn't update internal options and there
       is no set_option method, so abuse gobject data slots :-( */
    g_object_set_data_full(G_OBJECT(master),
                           "tEXt::Entangle::Orientation",
			   g_strdup_printf("%d", orient),
                           g_free);

    result = entangle_pixbuf_auto_rotate(master);

 cleanup:
    if (loader)
        g_object_unref(loader);
    if (preview)
        g_object_unref(preview);
    if (metadata)
        g_object_unref(metadata);
    return result;
}


static GdkPixbuf *entangle_pixbuf_open_image_preview(EntangleImage *image)
{
    if (entangle_pixbuf_is_raw(image)) {
        GdkPixbuf *result = entangle_pixbuf_open_image_preview_exiv(image, 256);
        if (!result)
            result = entangle_pixbuf_open_image_master_raw(image);
        return result;
    } else {
        return entangle_pixbuf_open_image_master_gdk(image);
    }
}


static GdkPixbuf *entangle_pixbuf_open_image_thumbnail(EntangleImage *image)
{
    GdkPixbuf *result = entangle_pixbuf_open_image_preview_exiv(image, 128);
    if (!result)
        result = entangle_pixbuf_open_image_master(image);
    return result;
}


GdkPixbuf *entangle_pixbuf_open_image(EntangleImage *image,
				      EntanglePixbufImageSlot slot)
{
    ENTANGLE_DEBUG("Open image %s %d", entangle_image_get_filename(image), slot);
    switch (slot) {
    case ENTANGLE_PIXBUF_IMAGE_SLOT_MASTER:
        return entangle_pixbuf_open_image_master(image);

    case ENTANGLE_PIXBUF_IMAGE_SLOT_PREVIEW:
        return entangle_pixbuf_open_image_preview(image);

    case ENTANGLE_PIXBUF_IMAGE_SLOT_THUMBNAIL:
        return entangle_pixbuf_open_image_thumbnail(image);

    default:
        g_warn_if_reached();
        return NULL;
    }
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
