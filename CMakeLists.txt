cmake_minimum_required(VERSION 3.20)
project(clapclap)

# Create compile_commands.json 
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add all cmake functionalities
file(GLOB CMAKE_ENTRIES "${CMAKE_SOURCE_DIR}/cmake/*.cmake")
foreach(ENTRY ${CMAKE_ENTRIES})
    include(${ENTRY})
endforeach()

# Common dependencies
add_subdirectory(common)

# Add all plugins
file(GLOB PLUGINS_ENTRIES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/plugins/*")
foreach(ENTRY ${PLUGINS_ENTRIES})
  if (IS_DIRECTORY ${ENTRY})
    add_subdirectory(${ENTRY})
  endif()
endforeach()
