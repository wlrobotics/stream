include(FindPackageHandleStandardArgs)

set(onvif_ROOT_DIR ${PROJECT_SOURCE_DIR}/3rdparty/onvif)
set(onvif_INCLUDES ${onvif_ROOT_DIR}/include)
set(onvif_LIBS ${onvif_ROOT_DIR}/lib/libonvif.a)

find_package_handle_standard_args(onvif DEFAULT_MSG onvif_LIBS onvif_INCLUDES)

