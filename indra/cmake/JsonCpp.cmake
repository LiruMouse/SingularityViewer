# -*- cmake -*-

include(Prebuilt)

set(JSONCPP_FIND_QUIETLY OFF)
set(JSONCPP_FIND_REQUIRED ON)

if (STANDALONE)
  include(FindJsonCpp)
else (STANDALONE)
  use_prebuilt_binary(jsoncpp)
  if (WINDOWS)
    if(MSVC12)
      set(JSONCPP_LIBRARIES 
        debug json_vc${MSVC_SUFFIX}debug_libmt.lib
        optimized json_vc${MSVC_SUFFIX}_libmt)
    else(MSVC12)
      set(JSONCPP_LIBRARIES 
        debug json_vc${MSVC_SUFFIX}d
        optimized json_vc${MSVC_SUFFIX})
    endif(MSVC12)
  elseif (DARWIN)
    set(JSONCPP_LIBRARIES json_linux-gcc-4.0.1_libmt)
  elseif (LINUX)
    set(JSONCPP_LIBRARIES jsoncpp)
  endif (WINDOWS)
  set(JSONCPP_INCLUDE_DIR
    ${LIBS_PREBUILT_DIR}/include/jsoncpp
    ${LIBS_PREBUILT_LEGACY_DIR}/include/jsoncpp
    )
endif (STANDALONE)
