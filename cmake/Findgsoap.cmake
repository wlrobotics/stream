include(FindPackageHandleStandardArgs)

set(gsoap_ROOT_DIR ${PROJECT_SOURCE_DIR}/3rdparty/gsoap)

add_definitions(-DWITH_DOM) 
add_definitions(-DWITH_OPENSSL)

set(gsoap_INCLUDES ${gsoap_ROOT_DIR}/include)
list(APPEND gsoap_INCLUDES ${gsoap_ROOT_DIR}/share/gsoap/plugin)

set(gsoap_LIBS ${gsoap_ROOT_DIR}/lib/libgsoapssl++.a)

find_package_handle_standard_args(gsoap DEFAULT_MSG gsoap_LIBS gsoap_INCLUDES)