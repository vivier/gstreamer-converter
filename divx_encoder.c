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
