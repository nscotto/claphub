get_filename_component(PLUGIN_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
# remove the second parameter to make the plugin build
add_plugin(${PLUGIN_NAME})


# this shouldn't be needed:
set(pffft_DIR ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/generators)
find_package(pffft REQUIRED)
target_link_libraries(${PLUGIN_NAME} pffft::pffft)
