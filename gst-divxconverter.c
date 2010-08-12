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

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

#include "divx_encoder.h"

typedef struct {
	GMainLoop *loop;
	GstElement *decoder;
	GstElement *encoder;
	guint audio_bitrate;
	guint video_bitrate;
	guint size;
	guint start;
	guint end;
} divx_info;

static gboolean cb_bus(GstBus *bus, GstMessage *msg, gpointer data)
{
	divx_info *divx = (divx_info *)data;

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_STATE_CHANGED: {
		GstState oldstate, newstate, pending;
		static done = 0;
		gst_message_parse_state_changed(msg, &oldstate, &newstate,
						&pending);
		if (divx->start != 0 &&
		    !done && newstate == GST_STATE_PLAYING &&
		    gst_element_seek (divx->decoder, 1.0,
				      GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                        	      GST_SEEK_TYPE_SET,
				      divx->start * GST_SECOND,
                         	      GST_SEEK_TYPE_NONE,
				      GST_CLOCK_TIME_NONE)) {
				done = 1;
		}
		break;
	}
	case GST_MESSAGE_EOS:
		g_main_loop_quit (divx->loop);
		break;
	case GST_MESSAGE_ERROR: {
		gchar *debug;
		GError *error;
		gst_message_parse_error (msg, &error, &debug);
		g_free (debug);
		g_printerr ("Error: %s\n", error->message);
		g_error_free (error);
		g_main_loop_quit (divx->loop);
		break;
	}
	default:
		break;
	}
	return TRUE;
}

static void set_divx_video_bitrate(GstBin *encoder, unsigned int bitrate)
{
	g_object_set (G_OBJECT (gst_bin_get_by_name(encoder, "video")),
		      "bitrate", bitrate, NULL);
}

static void set_divx_audio_bitrate(GstBin *encoder, unsigned int bitrate)
{
	g_object_set (G_OBJECT ( gst_bin_get_by_name(encoder, "audio")),
		      "target", 1, NULL);
	g_object_set (G_OBJECT (gst_bin_get_by_name(encoder, "audio")),
		      "bitrate", bitrate, NULL);
}

static void cb_newpad (GstElement *decodebin, GstPad *pad,
		       gboolean last, gpointer data)
{
	GstCaps *caps;
	divx_info *divx = data;
	GstPad *divxpad;

	caps = gst_pad_get_caps (pad);

	divxpad = gst_element_get_compatible_pad (divx->encoder, pad, caps);
	if (divxpad == NULL)
		return;

	gst_pad_link (pad, divxpad);

	if (divx->video_bitrate == 0 && divx->size != 0) {
		gint64 len = -1;
		gint64 target_size;
		gint64 audio_size;
		gint64 video_size;

		if (divx->end) {
			len = divx->end - divx->start;
		} else {
			GstFormat fmt = GST_FORMAT_TIME;
			gst_element_query_duration( divx->decoder, &fmt, &len);
			if (len == -1)
				return;

			len -= divx->start * GST_SECOND;
			len /= GST_SECOND;
		}

		target_size = (divx->size - 2) * 1024 * 1024;
		audio_size = len * divx->audio_bitrate * 1024 / 8;
		video_size = target_size - audio_size;

		divx->video_bitrate = video_size / (len  / 8);

		set_divx_audio_bitrate(GST_BIN(divx->encoder),
				       divx->audio_bitrate);

		set_divx_video_bitrate(GST_BIN(divx->encoder),
				       divx->video_bitrate);

		g_print("audio bitrate : %u kb/s\n", divx->audio_bitrate);
		g_print("video bitrate : %u  b/s\n", divx->video_bitrate);
	} else if (divx->size == 0) {

		set_divx_audio_bitrate((GstBin*)divx->encoder,
				       divx->audio_bitrate);

		set_divx_video_bitrate((GstBin*)divx->encoder,
				       divx->video_bitrate);
	}
}

static gboolean cb_print_position (divx_info *divx)
{
	GstFormat fmt = GST_FORMAT_TIME;
	gint64 pos, len, tmp;

	if (!gst_element_query_position (divx->decoder, &fmt, &pos)
            || pos == -1 )
		return TRUE;

	if (!gst_element_query_duration (divx->decoder, &fmt, &len)
	    || len == -1)
		return TRUE;

	if (divx->end)
		tmp = (divx->end - divx->start) * GST_SECOND;
	else
		tmp = len - divx->start * GST_SECOND;

	g_print ("Time: %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT "\r",
		 GST_TIME_ARGS (pos - divx->start * GST_SECOND), 
		 GST_TIME_ARGS (tmp));

	/* is this the end ? */

	if (divx->end != 0 && pos >= divx->end * GST_SECOND) {
		gst_element_seek (divx->decoder, 1.0,
				  GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
				  GST_SEEK_TYPE_SET, len,
				  GST_SEEK_TYPE_NONE,
				  GST_CLOCK_TIME_NONE);
	}

	/* call me again */
	return TRUE;
}


int main (int argc, char *argv[])
{
	GstElement *pipeline;
	GstBus *bus;
	GstElement *source, *decoder, *sink;
	divx_info info;
	GOptionContext *ctx;
	GError *err = NULL;
	GOptionEntry entries[] = {
		{ "audio-bitrate", 'a', 0, G_OPTION_ARG_INT,
		  &info.audio_bitrate, "Set audio bitrate kilobits/second",
		  NULL },
		{ "video-bitrate", 'v', 0, G_OPTION_ARG_INT,
		  &info.video_bitrate, "Set video bitrate bits/second",
		  NULL },
		{ "file-size", 's', 0, G_OPTION_ARG_INT,
		  &info.size, "Set file size in megabyte",
		  NULL },
		{ "start", 't', 0, G_OPTION_ARG_INT,
		  &info.start, "Set the second from where to start encoding",
		  NULL },
		{ "end", 'e', 0, G_OPTION_ARG_INT,
		  &info.end, "Set the second where to stop encoding",
		  NULL },
		{ NULL }
	};

	memset(&info, 0, sizeof(info));

	info.size = 700;
	info.audio_bitrate = 192;

	/* Gstreamer init */

	if (!g_thread_supported ())
		g_thread_init (NULL);

	ctx = g_option_context_new ("source dest");
	g_option_context_add_main_entries (ctx, entries, NULL);
	g_option_context_add_group (ctx, gst_init_get_option_group ());

	if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
		g_print ("Failed to initialize: %s\n", err->message);
		g_error_free (err);
		return 1;
	}

	if (info.video_bitrate) {
		info.size = 0;
	}

	gst_init (&argc, &argv);
	info.loop = g_main_loop_new (NULL, FALSE);

	/* create a pipeline */

	pipeline = gst_pipeline_new ("pipeline");

	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	gst_bus_add_watch (bus, cb_bus, &info);
	gst_object_unref (bus);

	/** create an encoder bin */

	info.encoder = create_divx_encoder();

	/* add decoder elements */

	/** source */

	source = gst_element_factory_make ("filesrc", "source");
	g_object_set (G_OBJECT (source), "location", argv[1], NULL);

	/** decoder */

	decoder = gst_element_factory_make ("decodebin", "decoder");
	g_signal_connect (decoder, "new-decoded-pad",
			  G_CALLBACK (cb_newpad), &info);
	info.decoder = pipeline;

	/* sink */

	sink = gst_element_factory_make ("filesink", "sink");
	g_object_set (G_OBJECT (sink), "location", argv[2], NULL);

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
