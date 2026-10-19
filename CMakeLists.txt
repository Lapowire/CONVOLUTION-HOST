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
  target_sources(ConvolutionHost PRIVATE src/plugin/PluginProcessor.cpp src/plugin/Parameters.cpp)
  target_link_libraries(ConvolutionHost PRIVATE ch_core juce::juce_audio_utils)
  target_compile_definitions(ConvolutionHost PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  if(BUILD_TESTING)
    add_executable(ch_plugin_tests tests/PluginTestMain.cpp tests/RealtimeGuard.cpp tests/ProcessorTests.cpp)
    target_include_directories(ch_plugin_tests PRIVATE src "${juce_SOURCE_DIR}/modules")
    target_compile_definitions(ch_plugin_tests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 $<$<CONFIG:Debug>:DEBUG=1;_DEBUG=1>)
    target_link_libraries(ch_plugin_tests PRIVATE ConvolutionHost)
    add_test(NAME plugin COMMAND ch_plugin_tests)
  endif()
endif()
