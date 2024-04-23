project(stream)
cmake_minimum_required(VERSION 3.1.3)
set(CMAKE_CXX_STANDARD 17)
#SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ToolKit_Root ${CMAKE_CURRENT_SOURCE_DIR}/src/ZLToolKit/src)
set(MediaKit_Root ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(MediaServer_Root ${CMAKE_CURRENT_SOURCE_DIR}/src/media-server)

include_directories(${ToolKit_Root})
include_directories(${MediaKit_Root})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)

find_package(jsoncpp REQUIRED)

link_directories("/usr/lib/")
link_directories("/usr/lib/x86_64-linux-gnu/")


include_directories("/usr/include/jsoncpp/")

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

list(APPEND LINK_LIB_LIST pthread jsoncpp)
list(APPEND LINK_LIB_LIST dl)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/server)

file(GLOB app_src_list ${CMAKE_CURRENT_SOURCE_DIR}/server/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/server/*.h
                       )
add_executable(stream ${app_src_list} ${MediaKit_src_list} ${ToolKit_src_list} ${src_mpeg})

target_link_libraries(stream ${LINK_LIB_LIST})
