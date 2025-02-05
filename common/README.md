# common/

This is the location for source files that are common across plugin project.

## How it works

- In [CMakeLists.txt](./CMakeLists.txt) is declared a `common` library interface that adds
the clap dependency to anything depending on `common`.
- If source files (*.cc) are detected, a static library is created from it and added to common.
- If only include files (*.h) are present, include directory is added to `common`

[Back to parent README](../README.md)
