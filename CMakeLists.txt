cmake_minimum_required(VERSION 3.25)
project(Dart)

option(DART_DUMP "Build MCAP playback mode" OFF)
option(DART_CAPTURE "Record MCAP files while using the camera" OFF)
option(DART_WITH_DAHENG "Build the optional Daheng camera backend" OFF)

if(DART_DUMP)
    message(FATAL_ERROR
        "The original DUMP playback branch has incompatible headers and data fields. "
        "It has not been repaired in this environment configuration. Use -DDART_DUMP=OFF.")
endif()

list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/.deps/flatbuffers-25.2.10")
find_package(FlatBuffers 25.2.10 EXACT CONFIG REQUIRED)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_NAME}_src)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_src})
target_compile_definitions(${PROJECT_NAME} PRIVATE DART_PROJECT_ROOT="${CMAKE_SOURCE_DIR}/")
if(DART_DUMP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DUMP)
endif()
if(DART_CAPTURE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CAPTURE)
endif()
if(DART_WITH_DAHENG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DART_WITH_DAHENG)
endif()

add_subdirectory(src/utility)
add_subdirectory(src/capture)
add_subdirectory(src/sensors)
add_subdirectory(src/detect)
add_subdirectory(src/track)
add_subdirectory(src/mouse)


target_link_libraries(${PROJECT_NAME} PRIVATE utility)
target_include_directories(${PROJECT_NAME} PRIVATE src/utility/inc)

target_link_libraries(${PROJECT_NAME} PRIVATE capture)
target_include_directories(${PROJECT_NAME} PRIVATE src/capture/inc)

target_include_directories(${PROJECT_NAME} PRIVATE src/threads/inc)

target_link_libraries(${PROJECT_NAME} PRIVATE sensors)
target_include_directories(${PROJECT_NAME} PRIVATE src/sensors/inc)

target_link_libraries(${PROJECT_NAME} PRIVATE detect)
target_include_directories(${PROJECT_NAME} PRIVATE src/detect/inc)

target_link_libraries(${PROJECT_NAME} PRIVATE track)
target_include_directories(${PROJECT_NAME} PRIVATE src/track/inc)

target_link_libraries(${PROJECT_NAME} PRIVATE mouse)
target_include_directories(${PROJECT_NAME} PRIVATE src/mouse/inc)


target_link_libraries(${PROJECT_NAME} PRIVATE -lpthread)
