project(stream)
cmake_minimum_required(VERSION 3.1.3)
set(CMAKE_CXX_STANDARD 17)
#SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(ToolKit_Root ${CMAKE_CURRENT_SOURCE_DIR}/src/ZLToolKit/src)
set(MediaKit_Root ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(MediaServer_Root ${CMAKE_CURRENT_SOURCE_DIR}/src/media-server)

include_directories(${ToolKit_Root})
include_directories(${MediaKit_Root})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)

find_package(onvif REQUIRED)
if (onvif_FOUND)
    message(STATUS "found library:${onvif_LIBS}  ${onvif_INCLUDES}")
    include_directories(${onvif_INCLUDES})
    list(APPEND LINK_LIB_LIST ${onvif_LIBS})
else()
    message(FATAL_ERROR "onvif not found")
endif ()

find_package(aws-sdk-cpp-s3 REQUIRED)
if (aws-sdk-cpp-s3_FOUND)
    message(STATUS "found library:${aws-sdk-cpp-s3_LIBS}  ${aws-sdk-cpp-s3_INCLUDES}")
    include_directories(${aws-sdk-cpp-s3_INCLUDES})
    list(APPEND LINK_LIB_LIST ${aws-sdk-cpp-s3_LIBS})
else()
    message(FATAL_ERROR "aws-sdk-cpp-s3 not found")
endif ()

find_package(curl REQUIRED)
if (curl_FOUND)
    include_directories(${curl_INCLUDES})
    list(APPEND LINK_LIB_LIST ${curl_LIBS})
else()
    message(FATAL_ERROR "curl未找到")
endif ()

find_package(gsoap REQUIRED)
if (gsoap_FOUND)
    include_directories(${gsoap_INCLUDES})
    list(APPEND LINK_LIB_LIST ${gsoap_LIBS})
else()
    message(FATAL_ERROR "gsoap not found")
endif ()

find_package(ffmpeg REQUIRED)
if (ffmpeg_FOUND)
    include_directories(${ffmpeg_INCLUDES})
    list(APPEND LINK_LIB_LIST ${ffmpeg_LIBS})
else()
    message(FATAL_ERROR "ffmpeg not found")
endif ()

find_package(HCNetSDK REQUIRED)
if (HCNetSDK_FOUND)
    include_directories(${HCNetSDK_INCLUDES})
    list(APPEND LINK_LIB_LIST ${HCNetSDK_LIBS})
else()
    message(FATAL_ERROR "HCNetSDK not found")
endif ()

find_package(grpc REQUIRED)
if (grpc_FOUND)
    include_directories(${grpc_INCLUDES})
    list(APPEND LINK_LIB_LIST ${grpc_LIBS})
else()
    message(FATAL_ERROR "grpc not found")
endif ()

find_package(zlib REQUIRED)
if (zlib_FOUND)
    include_directories(${zlib_INCLUDES})
    list(APPEND LINK_LIB_LIST ${zlib_LIBS})
else()
    message(FATAL_ERROR "zlib not found")
endif ()

find_package(openssl REQUIRED)
if (openssl_FOUND)
    include_directories(${openssl_INCLUDES})
    list(APPEND LINK_LIB_LIST ${openssl_LIBS})
else()
    message(FATAL_ERROR "openssl not found")
endif ()

find_package(jsoncpp REQUIRED)
if (jsoncpp_FOUND)
    include_directories(${jsoncpp_INCLUDES})
    list(APPEND LINK_LIB_LIST ${jsoncpp_LIBS})
else()
    message(FATAL_ERROR "jsoncpp not found")
endif ()

#find_package(tinyxml2 REQUIRED)
#if (tinyxml2_FOUND)
#    message(STATUS "tinyxml2 found library:\"${tinyxml2_FOUND}\"")
#    include_directories(${tinyxml2_INCLUDES})
#    link_libraries(${tinyxml2_LIBS})
#endif (tinyxml2_FOUND)

#if(${CMAKE_BUILD_TYPE} MATCHES "Release")
#    find_package(jemalloc REQUIRED)
#    if(jemalloc_FOUND)
#        include_directories(${jemalloc_INCLUDES})
#        list(APPEND LINK_LIB_LIST ${jemalloc_LIBS})
#    else()
#        message(FATAL_ERROR "jemalloc not found")
#    endif()
#endif()

include_directories(${MediaServer_Root}/libmpeg/include)
aux_source_directory(${MediaServer_Root}/libmpeg/include src_mpeg)
aux_source_directory(${MediaServer_Root}/libmpeg/source src_mpeg)

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
add_definitions(-D_X86_64_)
endif()

include_directories(${MediaServer_Root}/libmov/include)
include_directories(${MediaServer_Root}/libflv/include)

aux_source_directory(${MediaServer_Root}/libmov/include src_mov)
aux_source_directory(${MediaServer_Root}/libmov/source src_mov)	

aux_source_directory(${MediaServer_Root}/libflv/include src_flv)
aux_source_directory(${MediaServer_Root}/libflv/source src_flv)	

add_library(mov STATIC ${src_mov})
add_library(flv STATIC ${src_flv})
list(APPEND LINK_LIB_LIST mov flv)
list(APPEND CXX_API_TARGETS mov flv)

file(GLOB ToolKit_src_list ${ToolKit_Root}/*/*.cpp ${ToolKit_Root}/*/*.h ${ToolKit_Root}/*/*.c)
file(GLOB MediaKit_src_list ${MediaKit_Root}/*/*.cpp ${MediaKit_Root}/*/*.h ${MediaKit_Root}/*/*.c)

list(APPEND LINK_LIB_LIST pthread)
list(APPEND LINK_LIB_LIST dl)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/server)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/server/RPC/proto)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/server/RPC/proto/etcd)

file(GLOB app_src_list ${CMAKE_CURRENT_SOURCE_DIR}/server/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/server/*.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/server/RPC/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/server/RPC/*.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/server//RPC/proto/*.cc ${CMAKE_CURRENT_SOURCE_DIR}/server//RPC/proto/*.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/server//RPC/proto/etcd/gogoproto/*.cc ${CMAKE_CURRENT_SOURCE_DIR}/server//RPC/proto/etcd/gogoproto/*.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/server//RPC/proto/etcd/google/api/*.cc ${CMAKE_CURRENT_SOURCE_DIR}/server//RPC/proto/etcd/google/api/*.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/server//RPC/proto/etcd/*.cc ${CMAKE_CURRENT_SOURCE_DIR}/server//RPC/proto/etcd/*.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/server/Device/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/server/Device/*.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/server/Record/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/server/Record/*.h
                       )

file(GLOB tinyxml_src ${CMAKE_CURRENT_SOURCE_DIR}/server/tinyxml2/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/server/tinyxml2/*.h )

#add_link_options($<$<CONFIG:RELEASE>:-s>)
add_executable(stream ${tinyxml_src} ${app_src_list} ${MediaKit_src_list} ${ToolKit_src_list} ${src_mpeg})

target_link_libraries(stream ${LINK_LIB_LIST})