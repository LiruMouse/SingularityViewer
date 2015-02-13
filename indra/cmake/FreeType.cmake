# -*- cmake -*-
include(Prebuilt)

if (STANDALONE)
  include(FindPkgConfig)

  pkg_check_modules(FREETYPE REQUIRED freetype2)
else (STANDALONE)
  use_prebuilt_binary(freetype)
  if(MSVC12)
    set(FREETYPE_INCLUDE_DIRS
      ${LIBS_PREBUILT_DIR}/include/freetype2
      ${LIBS_PREBUILT_LEGACY_DIR}/include/freetype2
      )
  else(MSVC12)
    set(FREETYPE_INCLUDE_DIRS
      ${LIBS_PREBUILT_DIR}/include
      ${LIBS_PREBUILT_LEGACY_DIR}/include
      )
  endif (MSVC12)

  set(FREETYPE_LIBRARIES freetype)
endif (STANDALONE)

link_directories(${FREETYPE_LIBRARY_DIRS})
