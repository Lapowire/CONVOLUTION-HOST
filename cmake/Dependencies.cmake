include(FetchContent)
set(CH_JUCE_REVISION 51a8a6d7aeae7326956d747737ccf1575e61e209)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(JUCE
  URL https://github.com/juce-framework/JUCE/archive/${CH_JUCE_REVISION}.tar.gz
  URL_HASH SHA256=20b740351cc030086951d87614bc08b42d072f1e4db68db1ef1d988e66dbbdbe)
FetchContent_MakeAvailable(JUCE)

if(CH_BUILD_PLUGIN AND CH_BUILD_CLAP)
  FetchContent_Declare(clap_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d
    GIT_SUBMODULES_RECURSE TRUE)
  set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(clap_extensions)
endif()
