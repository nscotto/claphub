# ClapHub 👏

A hub for developing all your C/C++ clap plugins, using CMake.

This is a template repository you can use to create a workspace to develop all your 
clap plugins in one place.

Key features:
- Add new plugins by copying the template in [plugins/template/](plugins/template/) to the plugins/
directory and follow the [instructions](/plugins/template/README.md) to enable it.
- Reuse sources amongst plugins by putting them in the [common/](common/) library.
- Add dependencies through cmake or declare new cmake functionalities by adding
*.cmake files to [cmake/](cmake/)


## Directory structure
```
.
├── cmake         # cmake functionalities
├── common        # common sources and dependencies
└── plugins       # plugins sub-projects 
    └── template
```

## Requirements

- [cmake](https://cmake.org/) >= 3.20 (arbitrarily set, lower probably works)
- a C++ compiler for your system

Optional:
- [ninja](https://ninja-build.org/) for faster building
- [conan](https://conan.io/) package manager for adding new dependencies


## Build the project

### Create build directory
Without [conan](https://conan.io/):
```shell
mkdir build/
```
With [conan](https://conan.io/) :
```shell
conan install . --build=missing
```
This will also install packages declared in [conanfile.txt](conanfile.txt).

### Configure the build

Configure build system (omit the `-G Ninja` to use regular `make`):
```shell
cd build
cmake .. -G Ninja  # pass extra options here
```

Currently available options:
- [CLAP_DIR](./cmake/add_plugin.cmake#L21): if set, destinations where to copy clap plugins after each build

Example configuring options:
```shell
cmake .. -G Ninja -DCLAP_DIR=~/.clap
```

### Build the project
Finally, to build the project, within `build/`:
```shell
cd build
cmake --build .
```

**Output: `build/plugins/${PLUGIN_NAME}/${PLUGIN_NAME}.clap`**

## Adding a new plugin

- Create a new directory in plugins and copy the [template](plugins/template/)
- Add extra dependencies with cmake by modifying the [CMakeLists.txt](plugins/template/CMakeLists.txt).
  By default, a target is created with the directory name under `${PLUGIN_NAME}`.
  You can add extra dependencies by referring to `${PLUGIN_NAME}`.
  
  e.g:
`plugins/my_fabulous_plugin/CMakeLists.txt:`
```cmake
get_filename_component(PLUGIN_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
add_plugin(${PLUGIN_NAME})

target_link_librairies(${PLUGIN_NAME} visage)
```
