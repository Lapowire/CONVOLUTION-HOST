# Exercise Windows-only validation logic without claiming a Windows compilation.
set(WIN32 TRUE)
set(APPLE FALSE)
set(CH_BUILD_PLUGIN ON CACHE BOOL "")
set(CH_BUILD_AAX ON CACHE BOOL "")
set(CH_BUILD_AU OFF CACHE BOOL "")
set(CH_AAX_SDK_PATH "${CMAKE_CURRENT_BINARY_DIR}/missing-aax-sdk" CACHE PATH "")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/PluginFormats.cmake")
