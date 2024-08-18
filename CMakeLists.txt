cmake_minimum_required(VERSION 3.10)
project(wgpu-cpp-wasm)

if(NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

set(CMAKE_FOLDER dependencies)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(ExternalProject)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/EmbedFile.cmake)

include(cmake/glm.cmake)
set(SDL_SHARED OFF CACHE INTERNAL "")
if(NOT EMSCRIPTEN)
  include(cmake/FetchDawn.cmake)
  add_subdirectory(third-party/sdl)
endif()

include(cmake/FetchSDL2WebGPU.cmake)
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

add_subdirectory(third-party/tree-sitter-cmake)

set(TDGE_TREE_SITTER_INCLUDES
        ${CMAKE_CURRENT_BINARY_DIR}/third-party/tree-sitter-cmake/tree-sitter/lib/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-cmake/include/
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/tree-sitter-cmake/tree-sitter-wgsl/
        CACHE INTERNAL "")

unset(CMAKE_FOLDER)
add_subdirectory(tdge)
