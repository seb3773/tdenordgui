cmake_minimum_required(VERSION 3.10)
project(NordVpnTQt3 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Extreme binary size reduction flags provided by user
set(MIN_SIZE_CXX_FLAGS "-g0 -Os -fstrict-aliasing -flto=auto -ffunction-sections -fdata-sections -fno-ident -fno-plt -fno-asynchronous-unwind-tables -fno-unwind-tables -fomit-frame-pointer -fno-stack-protector -fno-math-errno -ffast-math -fvisibility=hidden -fmerge-all-constants -Wno-deprecated-declaration")
set(MIN_SIZE_LINKER_FLAGS "-Wl,--relax -Wl,-z,norelro -fuse-ld=gold -Wl,--gc-sections,--build-id=none,--as-needed,--strip-all,-O1,--icf=all,--compress-debug-sections=zlib -s -flto=auto")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MIN_SIZE_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MIN_SIZE_LINKER_FLAGS}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)
pkg_check_modules(PROTOBUF REQUIRED protobuf)
pkg_check_modules(TQT REQUIRED tqt-mt)
pkg_check_modules(LIBNOTIFY libnotify REQUIRED)

add_definitions(-DQT_NO_VERSION_TAGGING)

find_program(PROTOC_COMPILER protoc REQUIRED)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)
find_program(TQMOC_EXECUTABLE NAMES tqmoc moc-tqt moc REQUIRED)

set(PROTO_DIR "${CMAKE_SOURCE_DIR}/../../protobuf")
set(PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/grpc_gen")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

file(GLOB_RECURSE PROTO_FILES "${PROTO_DIR}/daemon/*.proto")
# Exclude telemetry to avoid symbol collision with daemon/service.proto
list(FILTER PROTO_FILES EXCLUDE REGEX "telemetry")

set(PROTO_SRCS "")
set(PROTO_HDRS "")
set(PROTO_INC_DIRS "${PROTO_GEN_DIR}")

foreach(PROTO_FILE ${PROTO_FILES})
    set(PROTOC_ROOT "${PROTO_DIR}/daemon")
    file(RELATIVE_PATH REL_TO_ROOT "${PROTOC_ROOT}" "${PROTO_FILE}")
    string(REGEX REPLACE "\\.proto$" "" FIL_WE ${REL_TO_ROOT})

    # Keep relative path structure for output
    get_filename_component(OUT_SUBDIR "${FIL_WE}" DIRECTORY)
    if(NOT OUT_SUBDIR)
        set(OUT_SUBDIR ".")
    endif()

    set(PROTO_SRC "${PROTO_GEN_DIR}/${FIL_WE}.pb.cc")
    set(PROTO_HDR "${PROTO_GEN_DIR}/${FIL_WE}.pb.h")
    set(GRPC_SRC "${PROTO_GEN_DIR}/${FIL_WE}.grpc.pb.cc")
    set(GRPC_HDR "${PROTO_GEN_DIR}/${FIL_WE}.grpc.pb.h")

    list(APPEND PROTO_SRCS "${PROTO_SRC}" "${GRPC_SRC}")
    list(APPEND PROTO_HDRS "${PROTO_HDR}" "${GRPC_HDR}")

    get_filename_component(OUT_DIR "${PROTO_SRC}" DIRECTORY)
    file(MAKE_DIRECTORY "${OUT_DIR}")
    list(APPEND PROTO_INC_DIRS "${OUT_DIR}")

    add_custom_command(
        OUTPUT "${PROTO_SRC}" "${PROTO_HDR}" "${GRPC_SRC}" "${GRPC_HDR}"
        COMMAND ${PROTOC_COMPILER}
        ARGS --cpp_out="${PROTO_GEN_DIR}"
             --grpc_out="${PROTO_GEN_DIR}"
             --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN}"
             -I "${PROTOC_ROOT}"
             -I "${PROTO_DIR}"
             "${REL_TO_ROOT}"
        WORKING_DIRECTORY "${PROTOC_ROOT}"
        DEPENDS "${PROTO_FILE}"
    )
endforeach()

list(REMOVE_DUPLICATES PROTO_INC_DIRS)

include_directories(
    ${TQT_INCLUDE_DIRS}
    /usr/include/tqt
    /opt/trinity/include
    ${LIBNOTIFY_INCLUDE_DIRS}
    ${PROTO_INC_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)
link_directories(
    ${TQT_LIBRARY_DIRS}
    /opt/trinity/lib
    /opt/trinity/lib64
    ${LIBNOTIFY_LIBRARY_DIRS}
)

file(GLOB SOURCES "src/*.cpp")
if (NOT SOURCES)
    file(WRITE "src/main.cpp" "#include <tqapplication.h>\nint main(int argc, char** argv) { TQApplication app(argc, argv); return 0; }\n")
    set(SOURCES "src/main.cpp")
endif()

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/moc_daemon_client.cpp
    COMMAND ${TQMOC_EXECUTABLE} ${CMAKE_SOURCE_DIR}/src/daemon_client.h -o ${CMAKE_CURRENT_BINARY_DIR}/moc_daemon_client.cpp
    DEPENDS ${CMAKE_SOURCE_DIR}/src/daemon_client.h
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/moc_main_window.cpp
    COMMAND ${TQMOC_EXECUTABLE} ${CMAKE_SOURCE_DIR}/src/main_window.h -o ${CMAKE_CURRENT_BINARY_DIR}/moc_main_window.cpp
    DEPENDS ${CMAKE_SOURCE_DIR}/src/main_window.h
)

add_executable(tdenordgui ${SOURCES} ${PROTO_SRCS} 
    ${CMAKE_CURRENT_BINARY_DIR}/moc_daemon_client.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/moc_main_window.cpp)
target_compile_options(tdenordgui PRIVATE ${TQT_CFLAGS} -fpermissive)
target_link_libraries(tdenordgui ${TQT_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES} tdeui tdecore ${LIBNOTIFY_LIBRARIES})
