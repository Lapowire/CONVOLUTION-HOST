include(FetchContent)
set(CH_JUCE_REVISION 51a8a6d7aeae7326956d747737ccf1575e61e209)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG ${CH_JUCE_REVISION} GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(JUCE)
