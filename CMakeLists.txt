cmake_minimum_required(VERSION 3.23)

project(examples VERSION 1.0.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads)
link_libraries(Threads::Threads)

set(ENABLE_SOFTFLOAT OFF CACHE BOOL "Enable softfloat support")
set(ENABLE_INSTALL OFF CACHE BOOL "Enable installation")
set(ENABLE_TOOLS OFF CACHE BOOL "Enable tools")
add_subdirectory(eos-vm)

add_executable(driver src/main.cpp)
target_link_libraries(driver eos-vm)

add_executable(test_driver src/test_driver.cpp)
target_link_libraries(test_driver eos-vm)

add_subdirectory(contracts)