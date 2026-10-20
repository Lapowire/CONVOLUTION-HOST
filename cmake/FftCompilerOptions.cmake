# FFTW 3.3.10 places /bigobj in COMPILE_DEFINITIONS under MSVC.
# Preserve other definitions, moving this flag into compiler options.
function(ch_fix_fftw_msvc_flags target)
  get_target_property(definitions ${target} COMPILE_DEFINITIONS)
  if(definitions)
    list(REMOVE_ITEM definitions /bigobj)
    set_property(TARGET ${target} PROPERTY COMPILE_DEFINITIONS "${definitions}")
  endif()
  target_compile_options(${target} PRIVATE /bigobj)
endfunction()
