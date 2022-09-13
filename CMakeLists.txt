cmake_minimum_required (VERSION 3.18)
set(CMAKE_CXX_COMPILER /usr/bin/g++)

project(filemap
    LANGUAGES C
    VERSION 1.0.0
    )

message("Building with CMake version: ${CMAKE_VERSION}")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)

message("Compiling with ${CMAKE_CXX_COMPILER_ID}: ${CMAKE_CXX_COMPILER}")

add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCE_DIR}/filemap.c
    ${PROJECT_SOURCE_DIR}/control.c
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_include_directories(${PROJECT_NAME} PUBLIC
                          "${PROJECT_BINARY_DIR}"
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Credits to https://stackoverflow.com/a/69804923 
add_custom_target("uninstall" COMMENT "Uninstall installed files")
add_custom_command(
    TARGET "uninstall"
    POST_BUILD
    COMMENT "Uninstall files with install_manifest.txt"
    COMMAND xargs rm -vf < install_manifest.txt || echo Nothing in
            install_manifest.txt to be uninstalled!
)
