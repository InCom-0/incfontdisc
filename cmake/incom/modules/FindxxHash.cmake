#[=======================================================================[.rst:
FindxxHash
--------------

Find xxHash headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``xxHash::xxhash``
  The libxxhash library, if found.


Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project.
Note that one typically does not need to use these variables because lz4::lz4 is first class CMake target that encapsulates all of them.

``XXHASH_FOUND``
  true if (the requested version of) XXHASH is available.
``XXHASH_VERSION``
  the version of XXHASH.
``XXHASH_LIBRARY``
  the libraries to link against to use XXHASH.
``XXHASH_INCLUDE_DIR``
  where to find the LZ headers.
``XXHASH_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

# Try config package first (from xxHashConfig.cmake / xxHash-config.cmake)
set(_pkg "${CMAKE_FIND_PACKAGE_NAME}") # e.g. xxHash
set(_args CONFIG NO_MODULE)

if(${_pkg}_FIND_QUIETLY)
    list(APPEND _args QUIET)
endif()
if(${_pkg}_FIND_REQUIRED)
    list(APPEND _args REQUIRED)
endif()
if(DEFINED ${_pkg}_FIND_VERSION)
    list(APPEND _args ${${_pkg}_FIND_VERSION})
    if(${_pkg}_FIND_VERSION_EXACT)
        list(APPEND _args EXACT)
    endif()
endif()
if(DEFINED ${_pkg}_FIND_COMPONENTS)
    list(APPEND _args COMPONENTS ${${_pkg}_FIND_COMPONENTS})
endif()
if(DEFINED ${_pkg}_FIND_OPTIONAL_COMPONENTS)
    list(APPEND _args OPTIONAL_COMPONENTS ${${_pkg}_FIND_OPTIONAL_COMPONENTS})
endif()

find_package(${_pkg} ${_args})
if(${_pkg}_FOUND)
    # Config package found; stop here.
    set(XXHASH_FOUND TRUE)
    return()
endif()

# Config package NOT found; proceed with finding using PkgConfig.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(XXHASH_PC QUIET IMPORTED_TARGET libxxhash)
endif()

if(XXHASH_PC_FOUND)
    if(NOT TARGET xxHash::xxhash)
        add_library(xxHash::xxhash ALIAS PkgConfig::XXHASH_PC)
    endif()

    set(XXHASH_COMPILE_OPTIONS ${XXHASH_PC_CFLAGS_OTHER} CACHE STRING "libxxhash target compile options")
    set(XXHASH_VERSION ${XXHASH_PC_VERSION} CACHE STRING "libxxhash target version")

    find_path(XXHASH_INCLUDE_DIR
        NAMES xxhash.h
        HINTS ${XXHASH_PC_INCLUDEDIR} ${XXHASH_PC_INCLUDE_DIRS}
    )
    find_library(XXHASH_LIBRARY
        NAMES ${XXHASH_NAMES} libxxhash
        HINTS ${XXHASH_PC_LIBDIR} ${XXHASH_PC_LIBRARY_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(
        ${CMAKE_FIND_PACKAGE_NAME}
        REQUIRED_VARS XXHASH_LIBRARY XXHASH_INCLUDE_DIR
        VERSION_VAR XXHASH_VERSION
    )
    set(XXHASH_LIBRARIES ${XXHASH_LIBRARY})
    set(XXHASH_INCLUDE_DIRS ${XXHASH_INCLUDE_DIR})

else()
    message(STATUS "xxHash: libxxhash was not found via find_package() via FindxxHash.cmake module via PkgConfig.")
endif()

mark_as_advanced(
    XXHASH_LIBRARY
    XXHASH_INCLUDE_DIR
)
