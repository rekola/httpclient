# What a project that uses the installed library does: find the package, link the target, include the headers.
cmake_minimum_required(VERSION 3.20)
project(httpclient_consumer LANGUAGES CXX)
find_package(httpclient REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE httpclient::httpclient)
enable_testing()
add_test(NAME consumer COMMAND consumer)
