# Create interface with common dependencies
add_library(common INTERFACE)

target_link_libraries(common INTERFACE clap)

# Add common sources
get_all_sources(COMMON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
get_all_headers(COMMON_INC "${CMAKE_CURRENT_SOURCE_DIR}/include")
if (COMMON_SRC)
  add_library(common_lib STATIC ${COMMON_SRC})
  target_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(common INTERFACE common_lib)
elseif(COMMON_INC)
  target_include_directories(common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()


