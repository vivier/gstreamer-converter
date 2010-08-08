include(FindPkgConfig)

cmake_minimum_required(VERSION 2.8)

project(gst-converter)

pkg_search_module(GSTREAMER gstreamer-0.10)

include_directories(${GSTREAMER_INCLUDE_DIRS})

link_directories(${GSTREAMER_LIBRARY_DIRS})

add_executable(gst-divxconverter gst-divxconverter)

target_link_libraries(gst-divxconverter ${GSTREAMER_LIBRARIES})
