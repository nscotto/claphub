function(get_all_sources SOURCE_FILES DIRS)
  set(ALL_SOURCE_FILES "")

  foreach(DIR ${DIRS})
    file(GLOB SRC_FILES CONFIGURE_DEPENDS
      "${DIR}/*.c"
      "${DIR}/*.cc"
      "${DIR}/*.cpp"
    )
    list(APPEND ALL_SOURCE_FILES ${SRC_FILES})
  endforeach()

  set(${SOURCE_FILES} ${ALL_SOURCE_FILES} PARENT_SCOPE)
endfunction()

function(get_all_headers SOURCE_FILES DIRS)
  set(ALL_SOURCE_FILES "")

  foreach(DIR ${DIRS})
    file(GLOB SRC_FILES CONFIGURE_DEPENDS
      "${DIR}/*.h"
      "${DIR}/*.hpp"
      "${DIR}/*.inc"
    )
    list(APPEND ALL_SOURCE_FILES ${SRC_FILES})
  endforeach()

  set(${SOURCE_FILES} ${ALL_SOURCE_FILES} PARENT_SCOPE)
endfunction()
