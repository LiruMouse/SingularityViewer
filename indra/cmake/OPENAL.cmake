# -*- cmake -*-
include(Linking)
include(Prebuilt)

if(NOT FMOD)
if (LINUX)
  set(OPENAL ON CACHE BOOL "Enable OpenAL")
else (LINUX)
  set(OPENAL OFF CACHE BOOL "Enable OpenAL")
endif (LINUX)

if (OPENAL)
  if (STANDALONE)
    include(FindPkgConfig)
    include(FindOpenAL)
    pkg_check_modules(OPENAL_LIB REQUIRED openal)
    pkg_check_modules(FREEALUT_LIB REQUIRED freealut)
  else (STANDALONE)
    use_prebuilt_binary(openal-soft)
  endif (STANDALONE)
  set(OPENAL_LIBRARIES 
    openal
    alut
    )
    set(OPENAL_INCLUDE_DIRS
      ${LIBS_PREBUILT_DIR}/include
      ${LIBS_PREBUILT_LEGACY_DIR}/include
      )
endif (OPENAL)

if (OPENAL)
  message(STATUS "Building with OpenAL audio support")
  set(LLSTARTUP_COMPILE_FLAGS "${LLSTARTUP_COMPILE_FLAGS} -DLL_OPENAL")
endif (OPENAL)
endif(NOT FMOD)
