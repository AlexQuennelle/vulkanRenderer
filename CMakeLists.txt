cmake_minimum_required(VERSION 4.2.1)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_COLOR_DIAGNOSTICS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
message("Build type: ${CMAKE_BUILD_TYPE}")

option(CONFIG_USE_ASAN "Whether to enable address sanitizer" ON)
option(CONFIG_USE_UBSAN "Whether to enable undefined behavior sanitizer" ON)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

add_compile_options("-freflection")

if(CONFIG_USE_UBSAN)
    add_compile_options("-fsanitize=undefined")
elseif(CONFIG_USE_ASAN)
    add_compile_options("-fsanitize=address")
endif()

project(
    vulkanRenderer
    LANGUAGES CXX C
    VERSION 0.0
)

find_package(glfw3 REQUIRED)
find_package(glm   REQUIRED)
find_package(Vulkan REQUIRED)
find_package(assimp REQUIRED)
find_package(slang REQUIRED)
find_package(glaze REQUIRED)
set(glaze_ENABLE_REFLECTION26 ON)

add_executable("${CMAKE_PROJECT_NAME}")

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    assimp
    Vulkan::Vulkan
    glfw
    glaze::glaze
)

if(CONFIG_USE_UBSAN)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
elseif(CONFIG_USE_ASAN)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

set(CMAKE_CXX_FLAGS
    "-Wall -Wextra -Wnon-virtual-dtor -Wold-style-cast -Wcast-align\
    -Wunused -Woverloaded-virtual -Wpedantic -Wconversion -Wsign-conversion\
    -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wduplicated-cond\
    -Wduplicated-branches -Wlogical-op -Wuseless-cast -Wfatal-errors"
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
    VULKAN_HPP_NO_EXCEPTIONS
    GLFW_INCLUDE_VULKAN
)

target_compile_definitions(
  ${CMAKE_PROJECT_NAME} PUBLIC $<$<CONFIG:Release>:PRODUCTION_BUILD>
                               NAME="${CMAKE_PROJECT_NAME}"
)

set_target_properties(
  "${CMAKE_PROJECT_NAME}"
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
             RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/\
${CMAKE_PROJECT_NAME}_${CMAKE_PROJECT_VERSION}"
)

set(RELEASE_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin/\
${CMAKE_PROJECT_NAME}_${CMAKE_PROJECT_VERSION}/resources/"
)
set(DEBUG_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources/")
target_compile_definitions(
  ${CMAKE_PROJECT_NAME}
  PUBLIC
RESOURCES_PATH=$<IF:$<CONFIG:Release>,"./resources/","${DEBUG_RESOURCE_DIR}">
)

target_compile_features("${CMAKE_PROJECT_NAME}" PRIVATE cxx_std_26)
target_sources("${CMAKE_PROJECT_NAME}"
    PRIVATE
    src/main.cpp
    src/program.cpp
)
target_include_directories(
  "${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/"
)
target_include_directories(
  "${CMAKE_PROJECT_NAME}" PUBLIC "${PROJECT_BINARY_DIR}"
)

add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} # -E copy_directory
-DDEST_DIR=$<IF:$<CONFIG:Release>,"${RELEASE_RESOURCE_DIR}","${DEBUG_RESOURCE_DIR}">
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copyResources.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
