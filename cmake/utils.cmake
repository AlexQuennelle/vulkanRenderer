function (add_shader SOURCE)
    cmake_parse_arguments("SHADER" "" "" "SOURCES" ${ARGN})
    set(RELEASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_PROJECT_NAME}_${CMAKE_PROJECT_VERSION})
    set(SHADERS $<IF:$<CONFIG:Release>,${RELEASE_DIR}/shaders,${CMAKE_BINARY_DIR}/shaders>)
    set(ENTRY_POINTS -entry vertMain -entry fragMain)
    # TODO: Fix duplication of code
    add_custom_command(
        OUTPUT "${SHADERS}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADERS}"
    )
    add_custom_command(
        OUTPUT ${SHADERS}/${SOURCE}.spv
        COMMAND slangc ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SOURCE}.spv
        WORKING_DIRECTORY ${SHADERS}
        DEPENDS "${SHADERS}" ${SHADER_SOURCES}
        COMMENT "Compiling Shaders"
        VERBATIM
    )
    add_custom_target(${SOURCE} DEPENDS ${SHADERS}/${SOURCE}.spv)
endfunction()
