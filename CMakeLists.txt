#
# Copyright Laurent Vivier (2010/08/12)
#
#     Laurent@Vivier.eu
# 
# This software is governed by the CeCILL-C license under French law and
# abiding by the rules of distribution of free software.  You can  use, 
# modify and/ or redistribute the software under the terms of the CeCILL-C
# license as circulated by CEA, CNRS and INRIA at the following URL
# "http://www.cecill.info". 
# 
# As a counterpart to the access to the source code and  rights to copy,
# modify and redistribute granted by the license, users are provided only
# with a limited warranty  and the software's author,  the holder of the
# economic rights,  and the successive licensors  have only  limited
# liability. 
# 
# In this respect, the user's attention is drawn to the risks associated
# with loading,  using,  modifying and/or developing or reproducing the
# software by the user in light of its specific status of free software,
# that may mean  that it is complicated to manipulate,  and  that  also
# therefore means  that it is reserved for developers  and  experienced
# professionals having in-depth computer knowledge. Users are therefore
# encouraged to load and test the software's suitability as regards their
# requirements in conditions enabling the security of their systems and/or 
# data to be ensured and,  more generally, to use and operate it in the 
# same conditions as regards security. 
# 
# The fact that you are presently reading this means that you have had
# knowledge of the CeCILL-C license and that you accept its terms.
#

include(FindPkgConfig)

cmake_minimum_required(VERSION 2.8)

project(gst-converter)

pkg_search_module(GSTREAMER gstreamer-0.10)

include_directories(${GSTREAMER_INCLUDE_DIRS})

link_directories(${GSTREAMER_LIBRARY_DIRS})

add_executable(gst-divxconverter gst-divxconverter divx_encoder.c)

target_link_libraries(gst-divxconverter ${GSTREAMER_LIBRARIES})
