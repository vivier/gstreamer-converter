#include "divx_encoder.h"

GstElement *create_divx_encoder(void)
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
