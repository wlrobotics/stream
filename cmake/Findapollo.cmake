include(FindPackageHandleStandardArgs)

set(apollo_ROOT_DIR ${PROJECT_SOURCE_DIR}/3rdparty/apollo)

if (NOT EXISTS ${apollo_ROOT_DIR})
    message(FATAL_ERROR "apollo download error!")
endif ()

set(apollo_INCLUDES ${apollo_ROOT_DIR}/include)
list(APPEND apollo_LIBS ${apollo_ROOT_DIR}/lib/libapollo.a)

find_package_handle_standard_args(apollo DEFAULT_MSG apollo_LIBS apollo_INCLUDES)