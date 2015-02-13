# -*- cmake -*-
include(Prebuilt)

set(EXPAT_FIND_QUIETLY ON)
set(EXPAT_FIND_REQUIRED ON)

if (STANDALONE)
  include(FindEXPAT)
else (STANDALONE)
    use_prebuilt_binary(expat)
    if (WINDOWS)
        if (MSVC12)
          set(EXPAT_LIBRARIES expat)
        else (MSVC12)
          set(EXPAT_LIBRARIES libexpatMT)
        endif (MSVC12)
    else (WINDOWS)
        set(EXPAT_LIBRARIES expat)
    endif (WINDOWS)
    set(EXPAT_INCLUDE_DIRS
      ${LIBS_PREBUILT_DIR}/include
      ${LIBS_PREBUILT_LEGACY_DIR}/include
      )
endif (STANDALONE)
