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

#include <glib.h>
#include <stdio.h>
#include <unistd.h>

#include "entangle-debug.h"
#include "entangle-camera.h"
#include "entangle-params.h"
#include "entangle-control-button.h"
#include "entangle-control-choice.h"
#include "entangle-control-date.h"
#include "entangle-control-group.h"
#include "entangle-control-range.h"
#include "entangle-control-text.h"
#include "entangle-control-toggle.h"

#define ENTANGLE_CAMERA_GET_PRIVATE(obj)                                    \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), ENTANGLE_TYPE_CAMERA, EntangleCameraPrivate))

struct _EntangleCameraPrivate {
    EntangleParams *params;
    Camera *cam;

    CameraWidget *widgets;
    EntangleControlGroup *controls;

    EntangleProgress *progress;

    char *model;
    char *port;

    char *manual;
    char *summary;
    char *driver;

    gboolean hasCapture;
    gboolean hasPreview;
    gboolean hasSettings;
};

G_DEFINE_TYPE(EntangleCamera, entangle_camera, G_TYPE_OBJECT);

enum {
    PROP_0,
    PROP_MODEL,
    PROP_PORT,
    PROP_MANUAL,
    PROP_SUMMARY,
    PROP_DRIVER,
    PROP_PROGRESS,
    PROP_HAS_CAPTURE,
    PROP_HAS_PREVIEW,
    PROP_HAS_SETTINGS,
};


static void entangle_camera_get_property(GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    EntangleCamera *cam = ENTANGLE_CAMERA(object);
    EntangleCameraPrivate *priv = cam->priv;

    switch (prop_id)
        {
        case PROP_MODEL:
            g_value_set_string(value, priv->model);
            break;

        case PROP_PORT:
            g_value_set_string(value, priv->port);
            break;

        case PROP_MANUAL:
            g_value_set_string(value, priv->manual);
            break;

        case PROP_SUMMARY:
            g_value_set_string(value, priv->summary);
            break;

        case PROP_DRIVER:
            g_value_set_string(value, priv->driver);
            break;

        case PROP_PROGRESS:
            g_value_set_object(value, priv->progress);
            break;

        case PROP_HAS_CAPTURE:
            g_value_set_boolean(value, priv->hasCapture);
            break;

        case PROP_HAS_PREVIEW:
            g_value_set_boolean(value, priv->hasPreview);
            break;

        case PROP_HAS_SETTINGS:
            g_value_set_boolean(value, priv->hasSettings);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}

static void entangle_camera_set_property(GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    EntangleCamera *cam = ENTANGLE_CAMERA(object);
    EntangleCameraPrivate *priv = cam->priv;

    switch (prop_id)
        {
        case PROP_MODEL:
            g_free(priv->model);
            priv->model = g_value_dup_string(value);
            break;

        case PROP_PORT:
            g_free(priv->port);
            priv->port = g_value_dup_string(value);
            break;

        case PROP_PROGRESS:
            entangle_camera_set_progress(cam, g_value_get_object(value));
            break;

        case PROP_HAS_CAPTURE:
            priv->hasCapture = g_value_get_boolean(value);
            ENTANGLE_DEBUG("Set has capture %d", priv->hasCapture);
            break;

        case PROP_HAS_PREVIEW:
            priv->hasPreview = g_value_get_boolean(value);
            ENTANGLE_DEBUG("Set has preview %d", priv->hasPreview);
            break;

        case PROP_HAS_SETTINGS:
            priv->hasSettings = g_value_get_boolean(value);
            ENTANGLE_DEBUG("Set has settings %d", priv->hasSettings);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}


static void entangle_camera_finalize(GObject *object)
{
    EntangleCamera *camera = ENTANGLE_CAMERA(object);
    EntangleCameraPrivate *priv = camera->priv;

    ENTANGLE_DEBUG("Finalize camera %p", object);

    if (priv->progress)
        g_object_unref(priv->progress);
    if (priv->cam) {
        gp_camera_exit(priv->cam, priv->params->ctx);
        gp_camera_free(priv->cam);
    }
    if (priv->widgets)
        gp_widget_unref(priv->widgets);
    if (priv->controls)
        g_object_unref(priv->controls);
    entangle_params_free(priv->params);
    g_free(priv->driver);
    g_free(priv->summary);
    g_free(priv->manual);
    g_free(priv->model);
    g_free(priv->port);

    G_OBJECT_CLASS (entangle_camera_parent_class)->finalize (object);
}


static void entangle_camera_class_init(EntangleCameraClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = entangle_camera_finalize;
    object_class->get_property = entangle_camera_get_property;
    object_class->set_property = entangle_camera_set_property;


    g_signal_new("camera-file-added",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(EntangleCameraClass, camera_file_added),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE,
                 1,
                 ENTANGLE_TYPE_CAMERA_FILE);

    g_signal_new("camera-file-captured",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(EntangleCameraClass, camera_file_captured),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE,
                 1,
                 ENTANGLE_TYPE_CAMERA_FILE);

    g_signal_new("camera-file-previewed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(EntangleCameraClass, camera_file_previewed),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE,
                 1,
                 ENTANGLE_TYPE_CAMERA_FILE);

    g_signal_new("camera-file-downloaded",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(EntangleCameraClass, camera_file_downloaded),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE,
                 1,
                 ENTANGLE_TYPE_CAMERA_FILE);

    g_signal_new("camera-file-deleted",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(EntangleCameraClass, camera_file_deleted),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE,
                 1,
                 ENTANGLE_TYPE_CAMERA_FILE);


    g_object_class_install_property(object_class,
                                    PROP_MODEL,
                                    g_param_spec_string("model",
                                                        "Camera model",
                                                        "Model name of the camera",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
    g_object_class_install_property(object_class,
                                    PROP_PORT,
                                    g_param_spec_string("port",
                                                        "Camera port",
                                                        "Device port of the camera",
                                                         NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_object_class_install_property(object_class,
                                    PROP_SUMMARY,
                                    g_param_spec_string("summary",
                                                        "Camera summary",
                                                        "Camera summary",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_object_class_install_property(object_class,
                                    PROP_MANUAL,
                                    g_param_spec_string("manual",
                                                        "Camera manual",
                                                        "Camera manual",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_object_class_install_property(object_class,
                                    PROP_DRIVER,
                                    g_param_spec_string("driver",
                                                        "Camera driver info",
                                                        "Camera driver information",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_object_class_install_property(object_class,
                                    PROP_PROGRESS,
                                    g_param_spec_object("progress",
                                                        "Progress updater",
                                                        "Operation progress updater",
                                                        ENTANGLE_TYPE_PROGRESS,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_object_class_install_property(object_class,
                                    PROP_HAS_CAPTURE,
                                    g_param_spec_boolean("has-capture",
                                                         "Capture supported",
                                                         "Whether image capture is supported",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB));
    g_object_class_install_property(object_class,
                                    PROP_HAS_PREVIEW,
                                    g_param_spec_boolean("has-preview",
                                                         "Preview supported",
                                                         "Whether image preview is supported",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB));
    g_object_class_install_property(object_class,
                                    PROP_HAS_SETTINGS,
                                    g_param_spec_boolean("has-settings",
                                                         "Settings supported",
                                                         "Whether camera settings configuration is supported",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB));
    ENTANGLE_DEBUG("install prog done");

    g_type_class_add_private(klass, sizeof(EntangleCameraPrivate));
}


EntangleCamera *entangle_camera_new(const char *model,
                            const char *port,
                            gboolean hasCapture,
                            gboolean hasPreview,
                            gboolean hasSettings)
{
    return ENTANGLE_CAMERA(g_object_new(ENTANGLE_TYPE_CAMERA,
                                    "model", model,
                                    "port", port,
                                    "has-capture", hasCapture,
                                    "has-preview", hasPreview,
                                    "has-settings", hasSettings,
                                    NULL));
}


static void entangle_camera_init(EntangleCamera *picker)
{
    EntangleCameraPrivate *priv;

    priv = picker->priv = ENTANGLE_CAMERA_GET_PRIVATE(picker);
}


const char *entangle_camera_get_model(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;
    return priv->model;
}

const char *entangle_camera_get_port(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;
    return priv->port;
}


static unsigned int do_entangle_camera_progress_start(GPContext *ctx G_GNUC_UNUSED,
                                                  float target,
                                                  const char *format,
                                                  va_list args,
                                                  void *data)
{
    EntangleCamera *cam = data;
    EntangleCameraPrivate *priv = cam->priv;

    if (priv->progress)
        entangle_progress_start(priv->progress, target, format, args);

    return 0; /* XXX what is this actually useful for ? */
}

static void do_entangle_camera_progress_update(GPContext *ctx G_GNUC_UNUSED,
                                           unsigned int id G_GNUC_UNUSED,
                                           float current,
                                           void *data)
{
    EntangleCamera *cam = data;
    EntangleCameraPrivate *priv = cam->priv;

    if (priv->progress)
        entangle_progress_update(priv->progress, current);
}

static void do_entangle_camera_progress_stop(GPContext *ctx G_GNUC_UNUSED,
                                         unsigned int id G_GNUC_UNUSED,
                                         void *data)
{
    EntangleCamera *cam = data;
    EntangleCameraPrivate *priv = cam->priv;

    if (priv->progress)
        entangle_progress_stop(priv->progress);
}

static GPContextFeedback do_entangle_camera_progress_cancelled(GPContext *ctx G_GNUC_UNUSED,
                                                           void *data)
{
    EntangleCamera *cam = data;
    EntangleCameraPrivate *priv = cam->priv;

    ENTANGLE_DEBUG("Cancel check");

    if (!priv->progress)
        return GP_CONTEXT_FEEDBACK_CANCEL;

    if (entangle_progress_cancelled(priv->progress)) {
        ENTANGLE_DEBUG("yes");
        return GP_CONTEXT_FEEDBACK_CANCEL;
    }

    ENTANGLE_DEBUG("no");
    return GP_CONTEXT_FEEDBACK_OK;
}


gboolean entangle_camera_connect(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;
    int i;
    GPPortInfo port;
    CameraAbilities cap;
    CameraText txt;

    ENTANGLE_DEBUG("Conencting to cam");

    if (priv->cam != NULL)
        return TRUE;

    priv->params = entangle_params_new();

    gp_context_set_progress_funcs(priv->params->ctx,
                                  do_entangle_camera_progress_start,
                                  do_entangle_camera_progress_update,
                                  do_entangle_camera_progress_stop,
                                  cam);

    gp_context_set_cancel_func(priv->params->ctx,
                               do_entangle_camera_progress_cancelled,
                               cam);

    i = gp_port_info_list_lookup_path(priv->params->ports, priv->port);
    gp_port_info_list_get_info(priv->params->ports, i, &port);

    i = gp_abilities_list_lookup_model(priv->params->caps, priv->model);
    gp_abilities_list_get_abilities(priv->params->caps, i, &cap);

    gp_camera_new(&priv->cam);
    gp_camera_set_abilities(priv->cam, cap);
    gp_camera_set_port_info(priv->cam, port);

    if (gp_camera_init(priv->cam, priv->params->ctx) != GP_OK) {
        gp_camera_unref(priv->cam);
        priv->cam = NULL;
        ENTANGLE_DEBUG("failed");
        return FALSE;
    }

    /* Update entanglebilities as a sanity-check against orignal constructor */
    priv->hasCapture = priv->hasPreview = priv->hasSettings = FALSE;
    if (cap.operations & GP_OPERATION_CAPTURE_IMAGE)
        priv->hasCapture = TRUE;
    if (cap.operations & GP_OPERATION_CAPTURE_PREVIEW)
        priv->hasPreview = TRUE;
    if (cap.operations & GP_OPERATION_CONFIG)
        priv->hasSettings = TRUE;

    gp_camera_get_summary(priv->cam, &txt, priv->params->ctx);
    priv->summary = g_strdup(txt.text);

    gp_camera_get_manual(priv->cam, &txt, priv->params->ctx);
    priv->manual = g_strdup(txt.text);

    gp_camera_get_about(priv->cam, &txt, priv->params->ctx);
    priv->driver = g_strdup(txt.text);

    ENTANGLE_DEBUG("ok");
    return TRUE;
}

gboolean entangle_camera_disconnect(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;

    ENTANGLE_DEBUG("Disconnecting from cam");

    if (priv->cam == NULL)
        return TRUE;

    gp_camera_exit(priv->cam, priv->params->ctx);

    if (priv->widgets) {
        gp_widget_unref(priv->widgets);
        priv->widgets = NULL;
    }
    if (priv->controls) {
        g_object_unref(priv->controls);
        priv->controls = NULL;
    }

    g_free(priv->driver);
    g_free(priv->manual);
    g_free(priv->summary);
    priv->driver = priv->manual = priv->summary = NULL;

    entangle_params_free(priv->params);
    priv->params = NULL;

    gp_camera_unref(priv->cam);
    priv->cam = NULL;

    priv->hasCapture = priv->hasPreview = priv->hasSettings = FALSE;

    return TRUE;
}

gboolean entangle_camera_get_connected(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;

    return priv->cam != NULL ? TRUE : FALSE;
}

const char *entangle_camera_get_summary(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;

    return priv->summary;
}

const char *entangle_camera_get_manual(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;

    return priv->manual;
}

const char *entangle_camera_get_driver(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;

    return priv->driver;
}


EntangleCameraFile *entangle_camera_capture_image(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;
    CameraFilePath camerapath;
    EntangleCameraFile *file;

    if (!priv->cam) {
        ENTANGLE_DEBUG("Cannot capture image while not connected");
        return FALSE;
    }

    ENTANGLE_DEBUG("Starting capture");
    if (gp_camera_capture(priv->cam,
                          GP_CAPTURE_IMAGE,
                          &camerapath,
                          priv->params->ctx) != GP_OK)
        return NULL;

    file = entangle_camera_file_new(camerapath.folder,
                                camerapath.name);

    g_signal_emit_by_name(cam, "camera-file-captured", file);

    return file;
}


EntangleCameraFile *entangle_camera_preview_image(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;
    EntangleCameraFile *file;
    CameraFile *datafile = NULL;
    const char *mimetype = NULL;
    GByteArray *data = NULL;
    const char *rawdata;
    unsigned long int rawdatalen;
    const char *name;

    if (!priv->cam) {
        ENTANGLE_DEBUG("Cannot preview image while not connected");
        return FALSE;
    }

    gp_file_new(&datafile);

    ENTANGLE_DEBUG("Starting preview");
    if (gp_camera_capture_preview(priv->cam,
                                  datafile,
                                  priv->params->ctx) != GP_OK) {
        ENTANGLE_DEBUG("Failed capture");
        goto error;
    }

    if (gp_file_get_data_and_size(datafile, &rawdata, &rawdatalen) != GP_OK)
        goto error;

    if (gp_file_get_name(datafile, &name) != GP_OK)
        goto error;

    file = entangle_camera_file_new("/", name);

    if (gp_file_get_mime_type(datafile, &mimetype) == GP_OK)
        entangle_camera_file_set_mimetype(file, mimetype);

    data = g_byte_array_new();
    g_byte_array_append(data, (const guint8 *)rawdata, rawdatalen);

    entangle_camera_file_set_data(file, data);
    g_byte_array_unref(data);

    gp_file_unref(datafile);

    g_signal_emit_by_name(cam, "camera-file-previewed", file);

    return file;

 error:
    if (datafile)
        gp_file_unref(datafile);
    return NULL;
}


gboolean entangle_camera_download_file(EntangleCamera *cam,
                                   EntangleCameraFile *file)
{
    EntangleCameraPrivate *priv = cam->priv;
    CameraFile *datafile = NULL;
    const char *data;
    unsigned long int datalen;
    GByteArray *filedata;

    if (!priv->cam) {
        ENTANGLE_DEBUG("Cannot download file while not connected");
        return FALSE;
    }

    ENTANGLE_DEBUG("Downloading '%s' from '%s'",
               entangle_camera_file_get_name(file),
               entangle_camera_file_get_folder(file));

    gp_file_new(&datafile);

    ENTANGLE_DEBUG("Getting file data");
    if (gp_camera_file_get(priv->cam,
                           entangle_camera_file_get_folder(file),
                           entangle_camera_file_get_name(file),
                           GP_FILE_TYPE_NORMAL,
                           datafile,
                           priv->params->ctx) != GP_OK)
        goto error;

    ENTANGLE_DEBUG("Fetching data");
    if (gp_file_get_data_and_size(datafile, &data, &datalen) != GP_OK)
        goto error;

    filedata = g_byte_array_new();
    g_byte_array_append(filedata, (const guint8*)data, datalen);
    gp_file_unref(datafile);


    entangle_camera_file_set_data(file, filedata);
    g_byte_array_unref(filedata);

    g_signal_emit_by_name(cam, "camera-file-downloaded", file);

    return TRUE;

 error:
    ENTANGLE_DEBUG("Error");
    if (datafile)
        gp_file_unref(datafile);
    return FALSE;
}


gboolean entangle_camera_delete_file(EntangleCamera *cam,
                                 EntangleCameraFile *file)
{
    EntangleCameraPrivate *priv = cam->priv;

    if (!priv->cam) {
        ENTANGLE_DEBUG("Cannot delete file while not connected");
        return FALSE;
    }

    ENTANGLE_DEBUG("Deleting '%s' from '%s'",
               entangle_camera_file_get_name(file),
               entangle_camera_file_get_folder(file));

    if (gp_camera_file_delete(priv->cam,
                              entangle_camera_file_get_folder(file),
                              entangle_camera_file_get_name(file),
                              priv->params->ctx) != GP_OK)
        return FALSE;

    g_signal_emit_by_name(cam, "camera-file-deleted", file);

    return TRUE;
}


gboolean entangle_camera_event_flush(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;
    CameraEventType eventType;
    void *eventData;

    if (!priv->cam) {
        ENTANGLE_DEBUG("Cannot flush events while not connected");
        return FALSE;
    }

    ENTANGLE_DEBUG("Flushing events");

    do {
        if (gp_camera_wait_for_event(priv->cam, 10, &eventType, &eventData, priv->params->ctx) != GP_OK) {
            ENTANGLE_DEBUG("Failed event wait");
            return FALSE;
        }

    } while (eventType != GP_EVENT_TIMEOUT);

    return TRUE;
}


gboolean entangle_camera_event_wait(EntangleCamera *cam,
                                int waitms)
{
    EntangleCameraPrivate *priv = cam->priv;
    CameraEventType eventType;
    void *eventData;
    gboolean ret = TRUE;

    if (!priv->cam) {
        ENTANGLE_DEBUG("Cannot wait for events while not connected");
        return FALSE;
    }

    ENTANGLE_DEBUG("Waiting for events");

    do {
        if (gp_camera_wait_for_event(priv->cam, waitms, &eventType, &eventData, priv->params->ctx) != GP_OK) {
            ENTANGLE_DEBUG("Failed event wait");
            return FALSE;
        }

        switch (eventType) {
        case GP_EVENT_UNKNOWN:
            ENTANGLE_DEBUG("Unknown event '%s'", (char *)eventData);
            break;

        case GP_EVENT_TIMEOUT:
            ENTANGLE_DEBUG("Wait timed out");
            break;

        case GP_EVENT_FILE_ADDED: {
            CameraFilePath *camerapath = eventData;
            EntangleCameraFile *file;

            ENTANGLE_DEBUG("File added '%s' in '%s'", camerapath->name, camerapath->folder);

            file = entangle_camera_file_new(camerapath->folder,
                                        camerapath->name);

            g_signal_emit_by_name(cam, "camera-file-added", file);

            g_object_unref(file);
        }   break;

        case GP_EVENT_FOLDER_ADDED: {
            CameraFilePath *camerapath = eventData;

            ENTANGLE_DEBUG("Folder added '%s' in '%s'", camerapath->name, camerapath->folder);
        }   break;

        case GP_EVENT_CAPTURE_COMPLETE:
            ENTANGLE_DEBUG("Capture is complete");
            break;

        default:
            ENTANGLE_DEBUG("Unexpected event received %d", eventType);
            ret = FALSE;
            break;
        }
    } while (ret && eventType != GP_EVENT_TIMEOUT);

    return ret;
}


static void do_update_control_text(GObject *object,
                                   GParamSpec *param G_GNUC_UNUSED,
                                   void *data)
{
    EntangleCamera *cam = data;
    EntangleCameraPrivate *priv = cam->priv;
    char *text;
    char *path;
    int id;
    CameraWidget *widget;

    g_object_get(object, "path", &path, "id", &id, "value", &text, NULL);
    ENTANGLE_DEBUG("update of widget %s", path);

    if (gp_widget_get_child_by_id(priv->widgets, id, &widget) != GP_OK) {
        ENTANGLE_DEBUG("cannot get widget id %d", id);
        return;
    }

    if (gp_widget_set_value(widget, text) != GP_OK) {
        ENTANGLE_DEBUG("cannot set widget id %d to %s", id, text);
    }

    if (gp_camera_set_config(priv->cam, priv->widgets, priv->params->ctx) != GP_OK)
        ENTANGLE_DEBUG("cannot set config");

}

static void do_update_control_float(GObject *object,
                                    GParamSpec *param G_GNUC_UNUSED,
                                    void *data)
{
    EntangleCamera *cam = data;
    EntangleCameraPrivate *priv = cam->priv;
    float value;
    char *path;
    int id;
    CameraWidget *widget;

    g_object_get(object, "path", &path, "id", &id, "value", &value, NULL);
    ENTANGLE_DEBUG("update of widget %s", path);

    if (gp_widget_get_child_by_id(priv->widgets, id, &widget) != GP_OK) {
        ENTANGLE_DEBUG("cannot get widget id %d", id);
        return;
    }

    if (gp_widget_set_value(widget, &value) != GP_OK) {
        ENTANGLE_DEBUG("cannot set widget id %d to %f", id, value);
    }

    if (gp_camera_set_config(priv->cam, priv->widgets, priv->params->ctx) != GP_OK)
        ENTANGLE_DEBUG("cannot set config");

}

static void do_update_control_boolean(GObject *object,
                                      GParamSpec *param G_GNUC_UNUSED,
                                      void *data)
{
    EntangleCamera *cam = data;
    EntangleCameraPrivate *priv = cam->priv;
    gboolean value;
    char *path;
    int id;
    CameraWidget *widget;

    g_object_get(object, "path", &path, "id", &id, "value", &value, NULL);
    ENTANGLE_DEBUG("update of widget %s", path);

    if (gp_widget_get_child_by_id(priv->widgets, id, &widget) != GP_OK) {
        ENTANGLE_DEBUG("cannot get widget id %d", id);
        return;
    }

    if (gp_widget_set_value(widget, &value) != GP_OK) {
        ENTANGLE_DEBUG("cannot set widget id %d to %d", id, value);
    }

    if (gp_camera_set_config(priv->cam, priv->widgets, priv->params->ctx) != GP_OK)
        ENTANGLE_DEBUG("cannot set config");

}

static EntangleControl *do_build_controls(EntangleCamera *cam,
                                      const char *path,
                                      CameraWidget *widget)
{
    CameraWidgetType type;
    EntangleControl *ret = NULL;
    const char *name;
    char *fullpath;
    int id;
    const char *label;
    const char *info;

    if (gp_widget_get_type(widget, &type) != GP_OK)
        return NULL;

    if (gp_widget_get_name(widget, &name) != GP_OK)
        return NULL;

    gp_widget_get_id(widget, &id);
    gp_widget_get_label(widget, &label);
    gp_widget_get_info(widget, &info);
    if (info == NULL)
        info = label;

    fullpath = g_strdup_printf("%s/%s", path, name);

    switch (type) {
        /* We treat both window and section as just groups */
    case GP_WIDGET_WINDOW:
    case GP_WIDGET_SECTION:
        {
            EntangleControlGroup *grp;
            ENTANGLE_DEBUG("Add group %s %d %s", fullpath, id, label);
            grp = entangle_control_group_new(fullpath, id, label, info);
            for (int i = 0 ; i < gp_widget_count_children(widget) ; i++) {
                CameraWidget *child;
                EntangleControl *subctl;
                if (gp_widget_get_child(widget, i, &child) != GP_OK) {
                    g_object_unref(grp);
                    goto error;
                }
                if (!(subctl = do_build_controls(cam, fullpath, child))) {
                    g_object_unref(grp);
                    goto error;
                }

                entangle_control_group_add(grp, subctl);
            }

            ret = ENTANGLE_CONTROL(grp);
        } break;

    case GP_WIDGET_BUTTON:
        {
            ENTANGLE_DEBUG("Add button %s %d %s", fullpath, id, label);
            ret = ENTANGLE_CONTROL(entangle_control_button_new(fullpath, id, label, info));
        } break;

        /* Unclear why these two are the same in libgphoto */
    case GP_WIDGET_RADIO:
    case GP_WIDGET_MENU:
        {
            char *value = NULL;
            ENTANGLE_DEBUG("Add date %s %d %s", fullpath, id, label);
            ret = ENTANGLE_CONTROL(entangle_control_choice_new(fullpath, id, label, info));

            for (int i = 0 ; i < gp_widget_count_choices(widget) ; i++) {
                const char *choice;
                gp_widget_get_choice(widget, i, &choice);
                entangle_control_choice_add_entry(ENTANGLE_CONTROL_CHOICE(ret), choice);
            }

            gp_widget_get_value(widget, &value);
            g_object_set(ret, "value", value, NULL);
            g_signal_connect(ret, "notify::value",
                             G_CALLBACK(do_update_control_text), cam);
        } break;

    case GP_WIDGET_DATE:
        {
            int value = 0;
            ENTANGLE_DEBUG("Add date %s %d %s", fullpath, id, label);
            ret = ENTANGLE_CONTROL(entangle_control_date_new(fullpath, id, label, info));
            g_object_set(ret, "value", value, NULL);
        } break;

    case GP_WIDGET_RANGE:
        {
            float min, max, step;
            float value = 0.0;
            gp_widget_get_range(widget, &min, &max, &step);
            ENTANGLE_DEBUG("Add range %s %d %s %f %f %f", fullpath, id, label, min, max, step);
            ret = ENTANGLE_CONTROL(entangle_control_range_new(fullpath, id, label, info,
                                                      min, max, step));

            gp_widget_get_value(widget, &value);
            g_object_set(ret, "value", value, NULL);
            g_signal_connect(ret, "notify::value",
                             G_CALLBACK(do_update_control_float), cam);
        } break;

    case GP_WIDGET_TEXT:
        {
            char *value = NULL;
            ENTANGLE_DEBUG("Add date %s %d %s", fullpath, id, label);
            ret = ENTANGLE_CONTROL(entangle_control_text_new(fullpath, id, label, info));

            gp_widget_get_value(widget, &value);
            g_object_set(ret, "value", value, NULL);
            g_signal_connect(ret, "notify::value",
                             G_CALLBACK(do_update_control_text), cam);
        } break;

    case GP_WIDGET_TOGGLE:
        {
            int value = 0;
            ENTANGLE_DEBUG("Add date %s %d %s", fullpath, id, label);
            ret = ENTANGLE_CONTROL(entangle_control_toggle_new(fullpath, id, label, info));

            gp_widget_get_value(widget, &value);
            g_object_set(ret, "value", (gboolean)value, NULL);
            g_signal_connect(ret, "notify::value",
                             G_CALLBACK(do_update_control_boolean), cam);
        } break;
    }

 error:
    g_free(fullpath);
    return ret;
}


EntangleControlGroup *entangle_camera_get_controls(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;

    if (priv->cam == NULL)
        return NULL;

    if (priv->controls == NULL) {
        if (gp_camera_get_config(priv->cam, &priv->widgets, priv->params->ctx) != GP_OK)
            return NULL;

        priv->controls = ENTANGLE_CONTROL_GROUP(do_build_controls(cam, "", priv->widgets));
    }

    return priv->controls;
}

gboolean entangle_camera_get_has_capture(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;

    return priv->hasCapture;
}

gboolean entangle_camera_get_has_preview(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;

    return priv->hasPreview;
}

gboolean entangle_camera_get_has_settings(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;

    return priv->hasSettings;
}


void entangle_camera_set_progress(EntangleCamera *cam, EntangleProgress *prog)
{
    EntangleCameraPrivate *priv = cam->priv;

    if (priv->progress)
        g_object_unref(priv->progress);
    priv->progress = prog;
    if (priv->progress)
        g_object_ref(priv->progress);
}


EntangleProgress *entangle_camera_get_progress(EntangleCamera *cam)
{
    EntangleCameraPrivate *priv = cam->priv;

    return priv->progress;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
