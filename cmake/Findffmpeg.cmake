include(FindPackageHandleStandardArgs)

set(ffmpeg_ROOT_DIR ${PROJECT_SOURCE_DIR}/3rdparty/ffmpeg)

set(ffmpeg_INCLUDES ${ffmpeg_ROOT_DIR}/include)

set(ffmpeg_LIBS ${ffmpeg_ROOT_DIR}/lib/libavformat.a)
list(APPEND ffmpeg_LIBS ${ffmpeg_ROOT_DIR}/lib/libavcodec.a)
list(APPEND ffmpeg_LIBS ${ffmpeg_ROOT_DIR}/lib/libavutil.a)
list(APPEND ffmpeg_LIBS ${ffmpeg_ROOT_DIR}/lib/libswresample.a)

find_package_handle_standard_args(ffmpeg DEFAULT_MSG ffmpeg_LIBS ffmpeg_INCLUDES)

