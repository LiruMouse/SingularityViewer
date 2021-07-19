# -*- cmake -*-

include(OpenGL)

if (LLWINDOW_SDL2)
    use_prebuilt_binary(SDL2)
    set (SDL2_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/SDL2)
    set (SDL2_LIBRARY SDL2)
  include_directories(${SDL2_INCLUDE_DIR})
else (LLWINDOW_SDL2)
  if (LINUX)
    include(FindSDL)

    # This should be done by FindSDL.  Sigh.
    mark_as_advanced(
        SDLMAIN_LIBRARY
        SDL_INCLUDE_DIR
        SDL_LIBRARY
        )

    if (SDL_FOUND)
      add_definitions(-DLL_SDL=1)
      include_directories(${SDL_INCLUDE_DIR})
    endif (SDL_FOUND)
  endif(LINUX)
endif (LLWINDOW_SDL2)

set(LLWINDOW_INCLUDE_DIRS
    ${GLEXT_INCLUDE_DIR}
    ${LIBS_OPEN_DIR}/llwindow
    )

set(LLWINDOW_LIBRARIES
    llwindow
    )

if (WINDOWS)
    list(APPEND LLWINDOW_LIBRARIES
        comdlg32
        )
endif (WINDOWS)
