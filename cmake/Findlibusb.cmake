# Findlibusb
# -----------
#
# Find the libusb library.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
# ``LIBUSB_FOUND``
#   System has the libusb library.
# ``LIBUSB_INCLUDE_DIR``
#   The libusb include directory.
# ``LIBUSB_LIBRARY``
#   The libusb library.
# ``LIBUSB_LIBRARIES``
#   All libusb libraries.
#
# Hints
# ^^^^^
#
# Set ``LIBUSB_USE_STATIC_LIBS`` to ``TRUE`` to look for static libraries.

# Support preference of static libs by adjusting CMAKE_FIND_LIBRARY_SUFFIXES
if(LIBUSB_USE_STATIC_LIBS)
    set(_libusb_ORING_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()

find_package(PkgConfig QUIET)
pkg_check_modules(LIBUSB QUIET "libusb")

include(FindPackageHandleStandardArgs)

# libusb
find_path(LIBUSB_INCLUDE_DIR 
    NAMES 
        libusb-1.0/libusb.h
    )
find_library(LIBUSB_LIBRARY 
    NAMES
        usb-1.0
    )
find_package_handle_standard_args(LIBUSB
    DEFAULT_MSG
    LIBUSB_INCLUDE_DIR
    LIBUSB_LIBRARY
    )
mark_as_advanced(
    LIBUSB_INCLUDE_DIR
    LIBUSB_LIBRARY
    )

if(LIBUSB_FOUND)
    set(LIBUSB_LIBRARIES ${LIBUSB_LIBRARY})
    mark_as_advanced(LIBUSB_LIBRARIES)

    set(LIBUSB_INCLUDE_DIRS ${LIBUSB_INCLUDE_DIR})
    mark_as_advanced(LIBUSB_INCLUDE_DIRS)
endif()

# Restore the original find library ordering
if(LIBUSB_USE_STATIC_LIBS)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${_libusb_ORING_CMAKE_FIND_LIBRARY_SUFFIXES})
endif()
