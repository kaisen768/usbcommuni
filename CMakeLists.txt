cmake_minimum_required(VERSION 3.4)
cmake_policy(SET CMP0048 NEW)

project(usbcommuni LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${usbcommuni_SOURCE_DIR}/cmake")

add_compile_options(-O2 -std=gnu++11)

find_package(imobiledevice REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(libusb REQUIRED)

include_directories(
    ${usbcommuni_SOURCE_DIR}/src
    ${usbcommuni_SOURCE_DIR}/3rdparty
    ${IMOBILEDEVICE_INCLUDE_DIRS}
    ${LIBUSB_INCLUDE_DIR}
)

add_subdirectory(${usbcommuni_SOURCE_DIR}/3rdparty)
add_subdirectory(${usbcommuni_SOURCE_DIR}/src)

# exmaple
file(GLOB_RECURSE SOURCE_FILES ${usbcommuni_SOURCE_DIR}/example/example.cc)
add_executable(usbcommuniexample ${SOURCE_FILES})
target_link_libraries(usbcommuniexample 
    usbcommuni
    pthread
    dl
)

