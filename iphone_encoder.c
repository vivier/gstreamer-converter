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

#include "iphone_encoder.h"

GstElement *create_iphone_encoder(void)
{
	GstElement *iphone, *audio, *video, *mux, *parser;
	GstElement *audioqueue, *videoqueue;
	GstPad *audiopad, *videopad, *muxpad;
	GstElement *filter, *videoscale;
	GstCaps *filtercaps;

	iphone = gst_bin_new ("iphone-encoder");

	audio = gst_element_factory_make ("faac", "audio");
	g_object_set(G_OBJECT(audio), "profile", 2, NULL);

	videoscale = gst_element_factory_make ("videoscale", "videoscale");
	filter = gst_element_factory_make ("capsfilter", "flt");
	filtercaps = gst_caps_new_simple ("video/x-raw-yuv",
					  "width", G_TYPE_INT, 640,
					  "height", G_TYPE_INT, 480, NULL);
	g_object_set (G_OBJECT (filter), "caps", filtercaps, NULL);
	gst_caps_unref (filtercaps);

	video = gst_element_factory_make ("x264enc", "video");
	g_object_set(G_OBJECT(video), "cabac", 0, "ref", 2, "me", 2,
				      "bframes", 0, "subme", 6, "dct8x8", 0,
				      "pass", 5, "profile", 1,
                                      NULL);
	videoqueue = gst_element_factory_make ("queue", "video-queue");
	parser = gst_element_factory_make ("h264parse", "parser");

	audioqueue = gst_element_factory_make ("queue", "audio-queue");
	mux = gst_element_factory_make("qtmux", "muxer");

	gst_bin_add_many (GST_BIN (iphone), audioqueue, audio, videoqueue,
			  videoscale, filter, video, parser, mux, NULL);

	gst_element_link_many (audioqueue, audio, mux, NULL);
	gst_element_link_many (videoqueue, videoscale, filter,
			       video, parser, mux, NULL);

	audiopad = gst_element_get_static_pad (audioqueue, "sink");
	gst_element_add_pad (iphone,
			     gst_ghost_pad_new ("audio-sink", audiopad));
	gst_object_unref (audiopad);

	videopad = gst_element_get_static_pad (videoqueue, "sink");
	gst_element_add_pad (iphone,
			     gst_ghost_pad_new ("video-sink", videopad));
	gst_object_unref (videopad);

	muxpad = gst_element_get_static_pad(mux, "src");
	gst_element_add_pad (iphone, gst_ghost_pad_new ("src", muxpad));
	gst_object_unref (muxpad);

	return iphone;
}
