get_filename_component(PLUGIN_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
# remove the second parameter to make the plugin build
add_plugin(${PLUGIN_NAME} "don't build me")
