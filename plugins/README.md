# plugins/

This is where you add your plugins.

## Creating a new plugin using the template

- Copy the [template](./template) directory with a new name.
- Modify [CMakeLists.txt](./CMakeLists.txt) in your directory
from:
```cmake
get_filename_component(PLUGIN_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
# remove the second parameter to make the plugin build
add_plugin(${PLUGIN_NAME} "ignore me")
```
to:
```cmake
get_filename_component(PLUGIN_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
add_plugin(${PLUGIN_NAME})
```

This declares a plugin which name will be the name of the directory.
Alternatively, you can choose your pass another name to `add_plugin`.
