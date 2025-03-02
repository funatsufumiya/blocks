cmake_minimum_required(VERSION 3.24)
project(blocks)

set(BINARY_DIR ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})
make_directory(${BINARY_DIR})

add_subdirectory(lib/tinycthread)
add_subdirectory(lib/SDL)
add_executable(blocks WIN32
    lib/sqlite3/sqlite3.c
    lib/stb/stb.c
    src/block.c
    src/camera.c
    src/chunk.c
    src/database.c
    src/helpers.c
    src/main.c
    src/noise.c
    src/pipeline.c
    src/raycast.c
    src/voxel.c
    src/world.c
)
target_link_libraries(blocks PUBLIC SDL3::SDL3 tinycthread)
if(UNIX)
    target_link_libraries(blocks PUBLIC m)
endif()
target_include_directories(blocks PUBLIC lib/sqlite3)
target_include_directories(blocks PUBLIC lib/stb)
set_target_properties(blocks PROPERTIES C_STANDARD 11)

function(shader FILE)
    set(SOURCE shaders/${FILE})
    if(APPLE)
        set(OUTPUT ${BINARY_DIR}/${FILE}.msl)
    else()
        set(OUTPUT ${BINARY_DIR}/${FILE}.spv)
    endif()
    
    if(APPLE)
        set(INTERMEDIATE ${BINARY_DIR}/spirv_${FILE})
        add_custom_command(
            OUTPUT ${OUTPUT}
            COMMAND glslc ${SOURCE} -o ${INTERMEDIATE} -I src
            COMMAND spirv-cross ${INTERMEDIATE} --output ${OUTPUT} --msl --entry main
            COMMAND rm ${INTERMEDIATE}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS ${SOURCE} shaders/helpers.glsl src/config.h
            COMMENT ${SOURCE}
        )
    else()
        add_custom_command(
            OUTPUT ${OUTPUT}
            COMMAND glslc ${SOURCE} -o ${OUTPUT} -I src
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS ${SOURCE} shaders/helpers.glsl src/config.h
            COMMENT ${SOURCE}
        )
    endif()

    string(REPLACE . _ NAME ${FILE})
    add_custom_target(${NAME} DEPENDS ${OUTPUT})
    add_dependencies(blocks ${NAME})
endfunction()
shader(composite.frag)
shader(fullscreen.vert)
shader(opaque.frag)
shader(opaque.vert)
shader(random.frag)
shader(raycast.frag)
shader(raycast.vert)
shader(shadow.frag)
shader(shadow.vert)
shader(sky.frag)
shader(sky.vert)
shader(ssao.frag)
shader(transparent.frag)
shader(transparent.vert)
shader(ui.frag)

configure_file(LICENSE.txt ${BINARY_DIR} COPYONLY)
configure_file(README.md ${BINARY_DIR} COPYONLY)
configure_file(textures/atlas.png ${BINARY_DIR} COPYONLY)