include(FindPackageHandleStandardArgs)

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    set(HCNetSDK_ROOT_DIR ${PROJECT_SOURCE_DIR}/3rdparty/HCNetSDK)

    set(HCNetSDK_INCLUDES ${HCNetSDK_ROOT_DIR}/include)
    set(HCNetSDK_LIBS ${HCNetSDK_ROOT_DIR}/lib)

    set(HCNetSDK_LIBS ${HCNetSDK_ROOT_DIR}/lib/libhcnetsdk.so)
    list(APPEND HCNetSDK_LIBS ${HCNetSDK_ROOT_DIR}/lib/libHCCore.so)
    # list(APPEND HCNetSDK_LIBS ${HCNetSDK_ROOT_DIR}/lib/libssl.so)
    # list(APPEND HCNetSDK_LIBS ${HCNetSDK_ROOT_DIR}/lib/libcrypto.so)

    find_package_handle_standard_args(HCNetSDK DEFAULT_MSG HCNetSDK_LIBS HCNetSDK_INCLUDES)
else()
    set(HCNetSDK_INCLUDES ${PROJECT_SOURCE_DIR}/include)
    find_package_handle_standard_args(HCNetSDK HCNetSDK_INCLUDES)
endif()