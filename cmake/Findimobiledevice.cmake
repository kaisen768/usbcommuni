# Findimobiledevice
# -----------
#
# Find the IMobiledevice library.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
# ``IMOBILEDEVICE_FOUND``
#   System has the IMobiledevice library.
# ``IMOBILEDEVICE_INCLUDE_DIR``
#   The IMobiledevice include directory.
# ``IMOBILEDEVICE_LIBRARY``
#   The IMobiledevice library.
# ``IMOBILEDEVICE_LIBRARIES``
#   All IMobiledevice libraries.
#
# Hints
# ^^^^^
#
# Set ``IMOBILEDEVICE_USE_STATIC_LIBS`` to ``TRUE`` to look for static libraries.

# Support preference of static libs by adjusting CMAKE_FIND_LIBRARY_SUFFIXES
if(IMOBILEDEVICE_USE_STATIC_LIBS)
    set(_imobiledevice_ORING_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()

find_package(PkgConfig QUIET)
pkg_check_modules(IMOBILEDEVICE QUIET "libimobiledevice")

include(FindPackageHandleStandardArgs)

# imobiledevice
find_path(IMOBILEDEVICE_INCLUDE_DIR 
    NAMES 
        libimobiledevice/libimobiledevice.h
    )
find_library(IMOBILEDEVICE_LIBRARY 
    NAMES
        imobiledevice-1.0
    )
find_package_handle_standard_args(IMOBILEDEVICE
    DEFAULT_MSG
    IMOBILEDEVICE_INCLUDE_DIR
    IMOBILEDEVICE_LIBRARY
    )
mark_as_advanced(
    IMOBILEDEVICE_INCLUDE_DIR
    IMOBILEDEVICE_LIBRARY
    )

# imobiledevice-glue
find_path(IMOBILEDEVICE_GLUE_INCLUDE_DIR 
    NAMES 
        libimobiledevice-glue/collection.h
    )
find_library(IMOBILEDEVICE_GLUE_LIBRARY 
    NAMES
        imobiledevice-glue-1.0
    )
find_package_handle_standard_args(IMOBILEDEVICE_GLUE
    DEFAULT_MSG
    IMOBILEDEVICE_GLUE_INCLUDE_DIR
    IMOBILEDEVICE_GLUE_LIBRARY
    )
mark_as_advanced(
    IMOBILEDEVICE_GLUE_INCLUDE_DIR
    IMOBILEDEVICE_GLUE_LIBRARY
    )

# plist
find_path(PLIST_INCLUDE_DIR 
    NAMES 
        plist/plist.h
    )
find_library(PLIST_LIBRARY 
    NAMES
        plist-2.0
    )
find_package_handle_standard_args(PLIST
    DEFAULT_MSG
    PLIST_INCLUDE_DIR
    PLIST_LIBRARY
    )
mark_as_advanced(
    PLIST_INCLUDE_DIR
    PLIST_LIBRARY
    )

# usbmuxd
find_path(USBMUXD_INCLUDE_DIR 
    NAMES 
        usbmuxd.h
    )
find_library(USBMUXD_LIBRARY 
    NAMES
        usbmuxd-2.0
    )
find_package_handle_standard_args(USBMUXD
    DEFAULT_MSG
    USBMUXD_INCLUDE_DIR
    USBMUXD_LIBRARY
    )
mark_as_advanced(
    USBMUXD_INCLUDE_DIR
    USBMUXD_LIBRARY
    )

if(IMOBILEDEVICE_FOUND AND IMOBILEDEVICE_GLUE_FOUND AND PLIST_FOUND AND USBMUXD_FOUND)
    set(IMOBILEDEVICE_LIBRARIES 
            ${IMOBILEDEVICE_LIBRARY}
            ${IMOBILEDEVICE_GLUE_LIBRARY}
            ${PLIST_LIBRARY}
            ${USBMUXD_LIBRARY})
    mark_as_advanced(IMOBILEDEVICE_LIBRARIES)

    set(IMOBILEDEVICE_INCLUDE_DIRS 
            ${IMOBILEDEVICE_INCLUDE_DIR}
            ${IMOBILEDEVICE_GLUE_INCLUDE_DIR}
            ${PLIST_INCLUDE_DIR}
            ${USBMUXD_INCLUDE_DIR})
    mark_as_advanced(IMOBILEDEVICE_INCLUDE_DIRS)
endif()

# Restore the original find library ordering
if(IMOBILEDEVICE_USE_STATIC_LIBS)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${_imobiledevice_ORING_CMAKE_FIND_LIBRARY_SUFFIXES})
endif()
