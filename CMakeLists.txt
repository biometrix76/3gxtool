cmake_minimum_required(VERSION 3.1...3.14)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(3gxtool LANGUAGES CXX)

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    # Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" OFF)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/extern/yaml-cpp/CMakeLists.txt")
    message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/extern/dynalo/CMakeLists.txt")
    message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(extern/yaml-cpp)
add_subdirectory(extern/dynalo)

set(CMAKE_CXX_STANDARD 14)

include_directories(includes)
include_directories(yaml-cpp/include)
include_directories(yaml-cpp/include/yaml-cpp)
include_directories(yaml-cpp/include/yaml-cpp/contrib)
include_directories(yaml-cpp/include/yaml-cpp/node)
include_directories(yaml-cpp/include/yaml-cpp/node/detail)
include_directories(dynalo/include/dynalo)
include_directories(dynalo/include/dynalo/detail)
include_directories(dynalo/include/dynalo/windows)
include_directories(dynalo/include/dynalo/linux)
include_directories(dynalo/include/dynalo/macos)

add_executable(3gxtool
        includes/3gx.hpp
        includes/cxxopts.hpp
        includes/elf.hpp
        includes/ElfConvert.hpp
        includes/types.hpp
        sources/ElfConvert.cpp
        sources/main.cpp)

target_link_libraries(3gxtool PRIVATE yaml-cpp ${CMAKE_DL_LIBS})
target_include_directories(3gxtool PUBLIC extern/yaml-cpp/include/yaml-cpp)
target_include_directories(3gxtool PUBLIC extern/dynalo/include/dynalo)