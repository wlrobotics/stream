include(FindPackageHandleStandardArgs)

set(jemalloc_ROOT_DIR ${PROJECT_SOURCE_DIR}/3rdparty/jemalloc)

set(jemalloc_INCLUDES ${jemalloc_ROOT_DIR}/include)
list(APPEND jemalloc_LIBS ${jemalloc_ROOT_DIR}/lib/libjemalloc.a)

find_package_handle_standard_args(jemalloc DEFAULT_MSG jemalloc_LIBS jemalloc_INCLUDES)