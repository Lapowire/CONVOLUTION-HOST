cmake_minimum_required(VERSION 3.22)
project(ConvolutionHost VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(cmake/Dependencies.cmake)
include(CTest)
add_library(ch_core STATIC src/audio/RoutingModel.cpp src/ir/IrDecoder.cpp src/audio/ConvolutionEngine.cpp src/ir/IrLoader.cpp src/audio/ReverbProcessor.cpp src/ir/IrLibrary.cpp src/state/StateStore.cpp)
target_include_directories(ch_core PUBLIC src)
target_link_libraries(ch_core PUBLIC juce::juce_dsp juce::juce_cryptography juce::juce_data_structures)
# JUCE's fallback FFT may allocate scratch and acquire a spinlock during processing.
# Link FFTW directly on non-Apple platforms; Apple uses the native vDSP backend.
if(NOT APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFTW REQUIRED IMPORTED_TARGET fftw3f)
  target_link_libraries(ch_core PUBLIC PkgConfig::FFTW)
  target_compile_definitions(ch_core PUBLIC JUCE_DSP_USE_STATIC_FFTW=1)
endif()
target_compile_definitions(ch_core PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
if(BUILD_TESTING)
  add_executable(ch_tests tests/TestMain.cpp tests/RealtimeGuard.cpp tests/RoutingTests.cpp tests/IrDecoderTests.cpp tests/ConvolutionTests.cpp tests/IrLoaderTests.cpp tests/ReverbTests.cpp tests/LibraryTests.cpp tests/StateTests.cpp)
  target_link_libraries(ch_tests PRIVATE ch_core)
  add_test(NAME core COMMAND ch_tests)
endif()

option(CH_BUILD_PLUGIN "Build VST3 and Standalone" ON)
if(CH_BUILD_PLUGIN)
  juce_add_plugin(ConvolutionHost
    COMPANY_NAME Lapowire
    BUNDLE_ID com.lapowire.convolutionhost
    PLUGIN_MANUFACTURER_CODE Lpwr
    PLUGIN_CODE Cvht
    PRODUCT_NAME "CONVOLUTION HOST"
    FORMATS VST3 Standalone
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
  target_sources(ConvolutionHost PRIVATE src/plugin/PluginProcessor.cpp src/plugin/Parameters.cpp src/ui/PluginEditor.cpp src/ui/IrDisplay.cpp src/ui/LibraryPanel.cpp src/ui/RoutingPanel.cpp)
  target_link_libraries(ConvolutionHost PRIVATE ch_core juce::juce_audio_utils)
  target_compile_definitions(ConvolutionHost PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  if(BUILD_TESTING)
    add_executable(ch_plugin_tests tests/PluginTestMain.cpp tests/RealtimeGuard.cpp tests/ProcessorTests.cpp tests/EditorTests.cpp)
    target_include_directories(ch_plugin_tests PRIVATE src "${juce_SOURCE_DIR}/modules")
    target_compile_definitions(ch_plugin_tests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 $<$<CONFIG:Debug>:DEBUG=1;_DEBUG=1>)
    target_link_libraries(ch_plugin_tests PRIVATE ConvolutionHost)
    add_test(NAME plugin COMMAND ch_plugin_tests)
  endif()
endif()

if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_source_files_properties(tests/RealtimeGuard.cpp PROPERTIES COMPILE_DEFINITIONS CH_WRAP_ALLOCATIONS=1)
  foreach(test_target ch_tests ch_plugin_tests)
    if(TARGET ${test_target})
      target_link_options(${test_target} PRIVATE -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free)
    endif()
  endforeach()
endif()
