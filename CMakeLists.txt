cmake_minimum_required(VERSION 3.19)
project(Raspberry)

set(CMAKE_CXX_STANDARD 17)
find_package(SQLite3)

include(FindPkgConfig)
include(FetchContent)

include_directories(${SQLite3_INCLUDE_DIRS})

# Another way is to use CMake's FetchContent module to clone gRPC at
# configure time. This makes gRPC's source code available to your project,
# similar to a git submodule.
message(STATUS "Using gRPC via add_subdirectory (FetchContent).")
FetchContent_Declare(
        grpc
        GIT_REPOSITORY https://github.com/grpc/grpc.git
        # when using gRPC, you will actually set this to an existing tag, such as
        # v1.25.0, v1.26.0 etc..
        # For the purpose of testing, we override the tag used to the commit
        # that's currently under test.
        GIT_TAG v1.38.0)

set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(grpc)

# Since FetchContent uses add_subdirectory under the hood, we can use
# the grpc targets directly from this build.
set(_PROTOBUF_LIBPROTOBUF libprotobuf)
set(_REFLECTION grpc++_reflection)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
set(_GRPC_GRPCPP grpc++)
if (CMAKE_CROSSCOMPILING)
    find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else ()
    set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
endif ()

# Proto file
get_filename_component(hw_proto "../CentralProto/parkingspaces.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

# Generated sources
set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/parkingspaces.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/parkingspaces.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/parkingspaces.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/parkingspaces.grpc.pb.h")

add_custom_command(
        OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
        COMMAND ${_PROTOBUF_PROTOC}
        ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${hw_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${hw_proto}"
        DEPENDS "${hw_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

pkg_check_modules(CURLPP REQUIRED curlpp)

FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.7.3)

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
    FetchContent_Populate(json)
    add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_executable(Raspberry main.cpp database/database.h database/SQLDatabase.cpp database/SQLDatabase.h ${hw_proto_srcs}
        ${hw_grpc_srcs} server/parkingspacesimpl.cpp server/parkingspacesimpl.h server/parkingnotifications.cpp
        server/parkingnotifications.h server/server.h server/server.cpp conn_arduino/arduino_notification.h
        conn_arduino/firebase_notifications.cpp conn_arduino/firebase_notifications.h)

add_executable(RaspberryTest testclient/main.cpp ${hw_proto_srcs}  ${hw_grpc_srcs})

target_link_libraries(RaspberryTest ${SQLite3_LIBRARIES} ${_REFLECTION}
        ${_GRPC_GRPCPP}
        ${_PROTOBUF_LIBPROTOBUF}
        ${CURLPP_LDFLAGS}
        nlohmann_json::nlohmann_json)

target_link_libraries(Raspberry ${SQLite3_LIBRARIES} ${_REFLECTION}
        ${_GRPC_GRPCPP}
        ${_PROTOBUF_LIBPROTOBUF}
        ${CURLPP_LDFLAGS}
        nlohmann_json::nlohmann_json)