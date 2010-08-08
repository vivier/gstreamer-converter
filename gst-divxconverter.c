/*
 * cc $(pkg-config --libs --cflags gstreamer-0.10) -o gst-divxconverter gst-divxconverter.c
 */

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

typedef struct {
	GstElement *decoder;
	GstElement *encoder;
	guint audio_bitrate;
	guint video_bitrate;
	guint size;
} divx_info;

static gboolean cb_bus(GstBus *bus, GstMessage *msg, gpointer data)
{
	GMainLoop *loop = (GMainLoop *) data;

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_EOS:
		g_main_loop_quit (loop);
		break;
	case GST_MESSAGE_ERROR: {
		gchar *debug;
		GError *error;
		gst_message_parse_error (msg, &error, &debug);
		g_free (debug);
		g_printerr ("Error: %s\n", error->message);
		g_error_free (error);
		g_main_loop_quit (loop);
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

		GstFormat fmt = GST_FORMAT_TIME;
		gst_element_query_duration( divx->decoder, &fmt, &len);
		if (len == -1)
			return;

		len /= GST_SECOND;

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

static gboolean cb_print_position (GstElement *pipeline)
{
	GstFormat fmt = GST_FORMAT_TIME;
	gint64 pos, len;

	if (gst_element_query_position (pipeline, &fmt, &pos)
            && pos != -1 
	    && gst_element_query_duration (pipeline, &fmt, &len)
	    && len != -1) {
		g_print ("Time: %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT "\r",
		GST_TIME_ARGS (pos), GST_TIME_ARGS (len));
	}
	/* call me again */
	return TRUE;
}


static GstElement *create_divx_encoder(void)
{
	GstElement *divx, *audio, *video, *parser, *mux;
	GstElement *audioqueue, *videoqueue;
	GstPad *audiopad, *videopad, *muxpad;

	divx = gst_bin_new ("divx-encoder");

	audio = gst_element_factory_make ("lamemp3enc", "audio");
	parser = gst_element_factory_make ("mp3parse", "audio-parser");
	video = gst_element_factory_make ("ffenc_mpeg4", "video");
	videoqueue = gst_element_factory_make ("queue", "video-queue");
	audioqueue = gst_element_factory_make ("queue", "audio-queue");
	mux = gst_element_factory_make("avimux", "muxer");

	gst_bin_add_many (GST_BIN (divx), audioqueue, audio, videoqueue,
			  video, parser, mux, NULL);

	gst_element_link (audioqueue, audio);
	gst_element_link (audio, parser);
	gst_element_link (parser, mux);

	gst_element_link (videoqueue, video);
	gst_element_link (video, mux);

	audiopad = gst_element_get_static_pad (audioqueue, "sink");
	gst_element_add_pad (divx, gst_ghost_pad_new ("audio-sink", audiopad));
	gst_object_unref (audiopad);

	videopad = gst_element_get_static_pad (videoqueue, "sink");
	gst_element_add_pad (divx, gst_ghost_pad_new ("video-sink", videopad));
	gst_object_unref (videopad);

	muxpad = gst_element_get_static_pad(mux, "src");
	gst_element_add_pad (divx, gst_ghost_pad_new ("src", muxpad));
	gst_object_unref (muxpad);

	return divx;
}

int main (int argc, char *argv[])
{
	GMainLoop *loop;
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
	loop = g_main_loop_new (NULL, FALSE);

	/* create a pipeline */

	pipeline = gst_pipeline_new ("pipeline");

	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	gst_bus_add_watch (bus, cb_bus, loop);
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

	g_timeout_add (500, (GSourceFunc) cb_print_position, pipeline);
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
	g_main_loop_run (loop);

	/* cleanup */

	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (pipeline));

	g_print("\n");

	return 0;
}

