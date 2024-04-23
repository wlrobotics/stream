include(FindPackageHandleStandardArgs)

set(tinyxml2_ROOT_DIR ${PROJECT_SOURCE_DIR}/3rdparty/tinyxml2)

set(tinyxml2_INCLUDES ${tinyxml2_ROOT_DIR}/include)

list(APPEND tinyxml2_LIBS ${tinyxml2_ROOT_DIR}/lib/libtinyxml2.a)

find_package_handle_standard_args(tinyxml2 DEFAULT_MSG tinyxml2_LIBS tinyxml2_INCLUDES)