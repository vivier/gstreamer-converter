/*
 *
 * Copyright Laurent Vivier (2010/08/12)
 *
 *     Laurent@Vivier.eu
 * 
 * This software is governed by the CeCILL-C license under French law and
 * abiding by the rules of distribution of free software.  You can  use, 
 * modify and/ or redistribute the software under the terms of the CeCILL-C
 * license as circulated by CEA, CNRS and INRIA at the following URL
 * "http://www.cecill.info". 
 * 
 * As a counterpart to the access to the source code and  rights to copy,
 * modify and redistribute granted by the license, users are provided only
 * with a limited warranty  and the software's author,  the holder of the
 * economic rights,  and the successive licensors  have only  limited
 * liability. 
 * 
 * In this respect, the user's attention is drawn to the risks associated
 * with loading,  using,  modifying and/or developing or reproducing the
 * software by the user in light of its specific status of free software,
 * that may mean  that it is complicated to manipulate,  and  that  also
 * therefore means  that it is reserved for developers  and  experienced
 * professionals having in-depth computer knowledge. Users are therefore
 * encouraged to load and test the software's suitability as regards their
 * requirements in conditions enabling the security of their systems and/or 
 * data to be ensured and,  more generally, to use and operate it in the 
 * same conditions as regards security. 
 * 
 * The fact that you are presently reading this means that you have had
 * knowledge of the CeCILL-C license and that you accept its terms.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <gst/gst.h>

#include "iphone_encoder.h"

typedef struct {
	GMainLoop *loop;
	GstElement *decoder;
	GstElement *encoder;
} iphone_info;

static gboolean cb_bus(GstBus *bus, GstMessage *msg, gpointer data)
{
	iphone_info *iphone = (iphone_info *)data;

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_STATE_CHANGED: {
		break;
	}
	case GST_MESSAGE_EOS:
		g_main_loop_quit (iphone->loop);
		break;
	case GST_MESSAGE_ERROR: {
		gchar *debug;
		GError *error;
		gst_message_parse_error (msg, &error, &debug);
		g_free (debug);
		g_printerr ("Error: %s\n", error->message);
		g_error_free (error);
		g_main_loop_quit (iphone->loop);
		break;
	}
	default:
		break;
	}
	return TRUE;
}

static void cb_newpad (GstElement *decodebin, GstPad *pad,
		       gboolean last, gpointer data)
{
	GstCaps *caps;
	iphone_info *iphone = data;
	GstPad *iphonepad;

	caps = gst_pad_get_caps (pad);
	iphonepad = gst_element_get_compatible_pad (iphone->encoder, pad, caps);
	gst_caps_unref(caps);

	if (iphonepad == NULL)
		return;

	gst_pad_link (pad, iphonepad);
}

static gboolean cb_print_position (iphone_info *iphone)
{
	GstFormat fmt = GST_FORMAT_TIME;
	gint64 pos, len;

	if (!gst_element_query_position (iphone->decoder, &fmt, &pos)
            || pos == -1 )
		return TRUE;

	if (!gst_element_query_duration (iphone->decoder, &fmt, &len)
	    || len == -1)
		return TRUE;

	g_print ("Time: %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT "\r",
		 GST_TIME_ARGS (pos), GST_TIME_ARGS (len));

	/* call me again */
	return TRUE;
}


int main (int argc, char *argv[])
{
	GstElement *pipeline;
	GstBus *bus;
	GstElement *source, *decoder, *sink;
	gchar *srcfile, *dstfile;
	iphone_info info;
	GOptionContext *ctx;
	GError *err = NULL;
	gint force = 0;
	GOptionEntry entries[] = {
		{ "force", 'F', 0, G_OPTION_ARG_NONE,
		  &force, "Force file overwrite",
		  NULL },
		{ NULL }
	};

	memset(&info, 0, sizeof(info));

	/* Gstreamer init */

	if (!g_thread_supported ())
		g_thread_init (NULL);

	ctx = g_option_context_new ("source [dest]");
	g_option_context_add_main_entries (ctx, entries, NULL);
	g_option_context_add_group (ctx, gst_init_get_option_group ());

	if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
		g_print ("Failed to initialize: %s\n", err->message);
		g_error_free (err);
		return 1;
	}
	g_option_context_free(ctx);

	if (argc < 2) {
		g_printerr("Source file is missing.\n");
		return 1;
	}
	srcfile = argv[1];

	if (argc > 3) {
		g_printerr("Too many parameters.\n");
		return 1;
	}

	if (argc < 3) {
		gchar *suffix = strrchr(srcfile, '.');

		if (suffix == NULL) {
			dstfile = malloc(strlen(srcfile) + 5);
			strcpy(dstfile, srcfile);
		} else {
			dstfile = malloc(suffix - srcfile + 5);
			memcpy(dstfile, srcfile, suffix - srcfile);
			dstfile[suffix - srcfile] = 0;
		}
		strcat(dstfile, ".mp4");
		g_printerr("Destination file is \"%s\"\n", dstfile);
	} else {
		dstfile = argv[2];
	}

	if (!force && access(dstfile, F_OK) == 0) {
		g_printerr("\"%s\" already exists.\n", dstfile);
		return 1;
	} 

	gst_init (&argc, &argv);
	info.loop = g_main_loop_new (NULL, FALSE);

	/* create a pipeline */

	pipeline = gst_pipeline_new ("pipeline");

	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	gst_bus_add_watch (bus, cb_bus, &info);
	gst_object_unref (bus);

	/** create an encoder bin */

	info.encoder = create_iphone_encoder();

	/* add decoder elements */

	/** source */

	source = gst_element_factory_make ("filesrc", "source");
	g_object_set (G_OBJECT (source), "location", srcfile, NULL);

	/** decoder */

	decoder = gst_element_factory_make ("decodebin", "decoder");
	g_signal_connect (decoder, "new-decoded-pad",
			  G_CALLBACK (cb_newpad), &info);
	info.decoder = pipeline;

	/* sink */

	sink = gst_element_factory_make ("filesink", "sink");
	g_object_set (G_OBJECT (sink), "location", dstfile, NULL);

	/** link them together */

	gst_bin_add_many (GST_BIN (pipeline), source, decoder,
			  info.encoder, sink, NULL);

	gst_element_link (source, decoder);
	gst_element_link (info.encoder, sink);

	/* run */

	g_timeout_add (100, (GSourceFunc) cb_print_position, &info);
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
	g_main_loop_run (info.loop);

	/* cleanup */

	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (pipeline));

	g_print("\n");

	return 0;
}
