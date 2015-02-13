# -*- cmake -*-

include(Prebuilt)
include(Boost)

set(COLLADADOM_FIND_QUIETLY OFF)
set(COLLADADOM_FIND_REQUIRED ON)

if (STANDALONE)
  include (FindColladadom)
else (STANDALONE)
  use_prebuilt_binary(colladadom)

  if (NOT WINDOWS AND NOT LINUX)
	use_prebuilt_binary(pcre)
  endif (NOT WINDOWS AND NOT LINUX)

  if (NOT DARWIN AND NOT WINDOWS)
	use_prebuilt_binary(libxml)
  endif (NOT DARWIN AND NOT WINDOWS)

  set(COLLADADOM_INCLUDE_DIRS
    ${LIBS_PREBUILT_DIR}/include/collada
    ${LIBS_PREBUILT_DIR}/include/collada/1.4
    ${LIBS_PREBUILT_LEGACY_DIR}/include/collada
    ${LIBS_PREBUILT_LEGACY_DIR}/include/collada/1.4
    )

  if (WINDOWS)
	  if(MSVC12)
        use_prebuilt_binary(pcre)
        use_prebuilt_binary(libxml)
        set(COLLADADOM_LIBRARIES 
          debug libcollada14dom23-sd
          optimized libcollada14dom23-s
          libxml2_a
          debug pcrecppd
          optimized pcrecpp
          debug pcred
          optimized pcre
          ${BOOST_SYSTEM_LIBRARIES}
		  )
	  else(MSVC12)
        add_definitions(-DDOM_DYNAMIC)
        set(COLLADADOM_LIBRARIES 
		    debug libcollada14dom22-d
		    optimized libcollada14dom22
		    )
      endif(MSVC12)
  else (WINDOWS)
	  set(COLLADADOM_LIBRARIES 
		  collada14dom
		  minizip
		  xml2
		  )
  endif (WINDOWS)
endif (STANDALONE)

