include(FindPackageHandleStandardArgs)

set(jsoncpp_ROOT_DIR ${PROJECT_SOURCE_DIR}/3rdparty/jsoncpp)

set(jsoncpp_INCLUDES ${jsoncpp_ROOT_DIR}/include)
list(APPEND jsoncpp_LIBS ${jsoncpp_ROOT_DIR}/lib/libjsoncpp.a)

find_package_handle_standard_args(jsoncpp DEFAULT_MSG jsoncpp_LIBS jsoncpp_INCLUDES)