cmake_minimum_required(VERSION 3.15)
project(librpc)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

find_package(Threads REQUIRED)
find_package(flatbuffers REQUIRED)
find_package(spdlog REQUIRED)

add_library(rpc src/librpc.cpp src/TCPClient.cpp src/UDPClient.cpp src/mDNSDiscoveryService.cpp src/MPIMessageBuilder.cpp
        include/util/log.h)
target_include_directories(rpc
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(rpc PUBLIC flatbuffers::flatbuffers spdlog::spdlog)

set_property(TARGET rpc PROPERTY CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

install(TARGETS rpc DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
