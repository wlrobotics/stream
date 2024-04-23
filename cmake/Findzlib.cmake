include(FindPackageHandleStandardArgs)

set(zlib_ROOT_DIR ${PROJECT_SOURCE_DIR}/3rdparty/zlib)
set(zlib_INCLUDES ${zlib_ROOT_DIR}/include)
list(APPEND zlib_LIBS ${zlib_ROOT_DIR}/lib/libz.a)

find_package_handle_standard_args(zlib DEFAULT_MSG zlib_INCLUDES zlib_LIBS)


