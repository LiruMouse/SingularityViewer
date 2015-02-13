# -*- cmake -*-

include(Variables)

if (WINDOWS)
  if (WORD_SIZE EQUAL 32)
    set (DIRECTX_ARCHITECTURE x86)
  elseif (WORD_SIZE EQUAL 64)
    set (DIRECTX_ARCHITECTURE x64)
  else (WORD_SIZE EQUAL 32)
    set (DIRECTX_ARCHITECTURE x86)
  endif (WORD_SIZE EQUAL 32)

  SET(program_files $ENV{ProgramW6432})
  if(NOT program_files)
    SET(program_files $ENV{ProgramFiles})
  endif(NOT program_files)
  SET(program_files_x86 "ProgramFiles(x86)")
  SET(program_files_x86 $ENV{${program_files_x86}})

  find_path(DIRECTX_ROOT_DIR Include/dxdiag.h
            PATHS
            "$ENV{DXSDK_DIR}"
            "${program_files}/Microsoft DirectX SDK (June 2010)"
            "${program_files_x86}/Microsoft DirectX SDK (June 2010)"
            "${program_files}/Microsoft DirectX SDK (February 2010)"
            "${program_files_x86}/Microsoft DirectX SDK (February 2010)"
            "${program_files}/Microsoft DirectX SDK (March 2009)"
            "${program_files_x86}/Microsoft DirectX SDK (March 2009)"
            "${program_files}/Microsoft DirectX SDK (August 2008)"
            "${program_files_x86}/Microsoft DirectX SDK (August 2008)"
            "${program_files}/Microsoft DirectX SDK (June 2008)"
            "${program_files_x86}/Microsoft DirectX SDK (June 2008)"
            "${program_files}/Microsoft DirectX SDK (March 2008)"
            "${program_files_x86}/Microsoft DirectX SDK (March 2008)"
            "${program_files}/Microsoft DirectX SDK (November 2007)"
            "${program_files_x86}/Microsoft DirectX SDK (November 2007)"
            "${program_files}/Microsoft DirectX SDK (August 2007)"
            "${program_files_x86}/Microsoft DirectX SDK (August 2007)"
            )

  if (DIRECTX_ROOT_DIR)
    set (DIRECTX_INCLUDE_DIR "${DIRECTX_ROOT_DIR}/Include")
    set (DIRECTX_LIBRARY_DIR "${DIRECTX_ROOT_DIR}/Lib/${DIRECTX_ARCHITECTURE}")
  else (DIRECTX_ROOT_DIR)
    find_path (WIN_KIT_ROOT_DIR Include/um/windows.h
               PATHS
               "${program_files}/Windows Kits/8.1"
               "${program_files_x86}/Windows Kits/8.1"
               "${program_files}/Windows Kits/8.0"
               "${program_files_x86}/Windows Kits/8.0"
               )

    find_path (WIN_KIT_LIB_DIR dxguid.lib
               "${WIN_KIT_ROOT_DIR}/Lib/winv6.3/um/${DIRECTX_ARCHITECTURE}"
               "${WIN_KIT_ROOT_DIR}/Lib/Win8/um/${DIRECTX_ARCHITECTURE}"
               )

    if (WIN_KIT_ROOT_DIR AND WIN_KIT_LIB_DIR)
      set (DIRECTX_INCLUDE_DIR "${WIN_KIT_ROOT_DIR}/Include/um" "${WIN_KIT_ROOT_DIR}/Include/shared")
      set (DIRECTX_LIBRARY_DIR "${WIN_KIT_LIB_DIR}")
    endif (WIN_KIT_ROOT_DIR AND WIN_KIT_LIB_DIR)
  endif (DIRECTX_ROOT_DIR)

  if (DIRECTX_INCLUDE_DIR)
    include_directories(${DIRECTX_INCLUDE_DIR})
    if (DIRECTX_FIND_QUIETLY)
      message(STATUS "Found DirectX include: ${DIRECTX_INCLUDE_DIR}")
    endif (DIRECTX_FIND_QUIETLY)
  else (DIRECTX_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find DirectX SDK Include")
  endif (DIRECTX_INCLUDE_DIR)

  if (DIRECTX_LIBRARY_DIR)
    if (DIRECTX_FIND_QUIETLY)
      message(STATUS "Found DirectX include: ${DIRECTX_LIBRARY_DIR}")
    endif (DIRECTX_FIND_QUIETLY)
  else (DIRECTX_LIBRARY_DIR)
    message(FATAL_ERROR "Could not find DirectX SDK Libraries")
  endif (DIRECTX_LIBRARY_DIR)

endif (WINDOWS)
