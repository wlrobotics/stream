include(FindPackageHandleStandardArgs)

set(aws-sdk-cpp-s3_ROOT_DIR ${PROJECT_SOURCE_DIR}/3rdparty/aws-sdk-cpp-s3)

if (NOT EXISTS ${aws-sdk-cpp-s3_ROOT_DIR})
    message(FATAL_ERROR "aws-sdk-cpp-s3 download error!")
endif ()

set(aws-sdk-cpp-s3_INCLUDES ${aws-sdk-cpp-s3_ROOT_DIR}/include)

set(aws-sdk-cpp-s3_LIBS ${aws-sdk-cpp-s3_ROOT_DIR}/lib/libaws-cpp-sdk-s3.a)
list(APPEND aws-sdk-cpp-s3_LIBS ${aws-sdk-cpp-s3_ROOT_DIR}/lib/libaws-cpp-sdk-core.a)
list(APPEND aws-sdk-cpp-s3_LIBS ${aws-sdk-cpp-s3_ROOT_DIR}/lib/libaws-c-event-stream.a)
list(APPEND aws-sdk-cpp-s3_LIBS ${aws-sdk-cpp-s3_ROOT_DIR}/lib/libaws-c-common.a)
list(APPEND aws-sdk-cpp-s3_LIBS ${aws-sdk-cpp-s3_ROOT_DIR}/lib/libaws-checksums.a)

find_package_handle_standard_args(aws-sdk-cpp-s3 DEFAULT_MSG aws-sdk-cpp-s3_LIBS aws-sdk-cpp-s3_INCLUDES)

