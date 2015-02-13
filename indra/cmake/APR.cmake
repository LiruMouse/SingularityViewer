include(BerkeleyDB)
include(Linking)
include(Prebuilt)

set(APR_FIND_QUIETLY ON)
set(APR_FIND_REQUIRED ON)

set(APRUTIL_FIND_QUIETLY ON)
set(APRUTIL_FIND_REQUIRED ON)

if (STANDALONE)
  include(FindAPR)
else (STANDALONE)
  use_prebuilt_binary(apr_suite)
  if (WINDOWS)
    set(APR_LIBRARIES 
      debug libapr-1.lib
      optimized libapr-1.lib
      )
    set(APRICONV_LIBRARIES 
      debug libapriconv-1.lib
      optimized libapriconv-1.lib
      )
    set(APRUTIL_LIBRARIES 
      debug libaprutil-1.lib
      optimized libaprutil-1.lib
      )
  elseif (DARWIN)
    set(APR_LIBRARIES 
      debug libapr-1.0.dylib
      optimized libapr-1.0.dylib
      )
    set(APRUTIL_LIBRARIES
      debug libaprutil-1.dylib
      optimized libaprutil-1.dylib
      )
    set(APRICONV_LIBRARIES iconv)
  else (WINDOWS)
    set(APR_LIBRARIES apr-1)
    set(APRUTIL_LIBRARIES aprutil-1)
    set(APRICONV_LIBRARIES iconv)
  endif (WINDOWS)
  set(APR_INCLUDE_DIR
    ${LIBS_PREBUILT_DIR}/include/apr-1
    ${LIBS_PREBUILT_LEGACY_DIR}/include/apr-1
    )

  if (LINUX)
    list(APPEND APRUTIL_LIBRARIES ${DB_LIBRARIES})
  endif (LINUX)
endif (STANDALONE)
