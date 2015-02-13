# -*- cmake -*-

project(llaudio)

include(00-Common)
include(Audio)
include(LLAudio)
if (FMODSTUDIO)
  include(FMODSTUDIO)
endif (FMODSTUDIO)
if (FMODEX)
  include(FMODEX)
endif (FMODEX)
include(OPENAL)
include(LLCommon)
include(LLMath)
include(LLMessage)
include(LLVFS)

if (FMOD)
  include_directories(${FMOD_INCLUDE_DIR})
endif(FMOD)

include_directories(
    ${LLAUDIO_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLMESSAGE_INCLUDE_DIRS}
    ${LLVFS_INCLUDE_DIRS}
    ${OGG_INCLUDE_DIRS}
    ${VORBISENC_INCLUDE_DIRS}
    ${VORBISFILE_INCLUDE_DIRS}
    ${VORBIS_INCLUDE_DIRS}
    ${OPENAL_INCLUDE_DIRS}
    ${FREEAULT_LIB_INCLUDE_DIRS}
    )

set(llaudio_SOURCE_FILES
    llaudiodecodemgr.cpp
    llaudioengine.cpp
    lllistener.cpp
    llvorbisdecode.cpp
    llvorbisencode.cpp
    )

set(llaudio_HEADER_FILES
    CMakeLists.txt

    llaudiodecodemgr.h
    llaudioengine.h
    lllistener.h
    llvorbisdecode.h
    llvorbisencode.h
    llwindgen.h
    )

if (FMODSTUDIO)
    list(APPEND llaudio_SOURCE_FILES
         llaudioengine_fmodstudio.cpp
         lllistener_fmodstudio.cpp
         llstreamingaudio_fmodstudio.cpp
         )

    list(APPEND llaudio_HEADER_FILES
         llaudioengine_fmodstudio.h
         lllistener_fmodstudio.h
         llstreamingaudio_fmodstudio.h
         )
endif (FMODSTUDIO)

if (FMODEX)
    list(APPEND llaudio_SOURCE_FILES
         llaudioengine_fmodex.cpp
         lllistener_fmodex.cpp
         llstreamingaudio_fmodex.cpp
         )

    list(APPEND llaudio_HEADER_FILES
         llaudioengine_fmodex.h
         lllistener_fmodex.h
         llstreamingaudio_fmodex.h
         )
endif (FMODEX)

if (OPENAL)
  list(APPEND llaudio_SOURCE_FILES
    llaudioengine_openal.cpp
    lllistener_openal.cpp
    )
  
  list(APPEND llaudio_HEADER_FILES
    llaudioengine_openal.h
    lllistener_openal.h
    )
endif (OPENAL)

set_source_files_properties(${llaudio_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llaudio_SOURCE_FILES ${llaudio_HEADER_FILES})

add_library (llaudio ${llaudio_SOURCE_FILES})
add_dependencies(llaudio prepare)

