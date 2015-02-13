# -*- cmake -*-
if(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
set(${CMAKE_CURRENT_LIST_FILE}_INCLUDED "YES")

include(Variables)

if (NOT STANDALONE)

  if(CMAKE_BUILD_TYPE)
    string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_LOWER)
  endif(CMAKE_BUILD_TYPE)

  if(WINDOWS OR ${CMAKE_GENERATOR} MATCHES "Xcode")
    # the cmake xcode and VS generators implicitly append ${CMAKE_CFG_INTDIR} to the library paths for us
    # fortunately both windows and darwin are case insensitive filesystems so this works.
    set(ARCH_PREBUILT_LINK_DIRS 
      ${LIBS_PREBUILT_DIR}/lib
      ${LIBS_PREBUILT_LEGACY_DIR}/lib
	)
  else(WINDOWS OR ${CMAKE_GENERATOR} MATCHES "Xcode")
    # else block is for linux and any other makefile based generators
    set(ARCH_PREBUILT_LINK_DIRS 
      ${LIBS_PREBUILT_DIR}/lib/${CMAKE_BUILD_TYPE_LOWER}
      ${LIBS_PREBUILT_LEGACY_DIR}/lib/${CMAKE_BUILD_TYPE_LOWER}
	)
  endif(WINDOWS OR ${CMAKE_GENERATOR} MATCHES "Xcode")

  if (NOT "${CMAKE_BUILD_TYPE_LOWER}" STREQUAL "release")
    # When we're building something other than Release, append the
    # packages/lib/release directory to deal with autobuild packages that don't
    # provide (e.g.) lib/debug libraries.
    list(APPEND ARCH_PREBUILT_LINK_DIRS
      ${LIBS_PREBUILT_DIR}/lib/release
      ${LIBS_PREBUILT_LEGACY_DIR}/lib/release
   	)
  endif (NOT "${CMAKE_BUILD_TYPE_LOWER}" STREQUAL "release")
endif (NOT STANDALONE)

link_directories(${ARCH_PREBUILT_LINK_DIRS})

if (LINUX)
  set(DL_LIBRARY dl)
  set(PTHREAD_LIBRARY pthread)
else (LINUX)
  set(DL_LIBRARY "")
  set(PTHREAD_LIBRARY "")
endif (LINUX)

if (WINDOWS)	
  set(WINDOWS_LIBRARIES
      advapi32
      shell32
      ws2_32
      mswsock
      psapi
      winmm
      netapi32
      wldap32
      gdi32
      user32
      dbghelp
      )
else (WINDOWS)
  set(WINDOWS_LIBRARIES "")
endif (WINDOWS)
    
mark_as_advanced(DL_LIBRARY PTHREAD_LIBRARY WINDOWS_LIBRARIES)

endif(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
