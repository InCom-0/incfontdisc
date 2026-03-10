#[=======================================================================[.rst:
FindBLAKE2
--------------

Find libb2 headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``libb2::libb2``
  The liblibb2 library, if found.


Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project.
Note that one typically does not need to use these variables because libb2::libb2 is first class CMake target that encapsulates all of them.

``BLAKE2_FOUND``
  true if (the requested version of) BLAKE2 is available.
``BLAKE2_VERSION``
  the version of BLAKE2.
``BLAKE2_LIBRARY``
  the libraries to link against to use BLAKE2.
``BLAKE2_INCLUDE_DIR``
  where to find the LZ headers.
``BLAKE2_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(BLAKE2_PC QUIET IMPORTED_TARGET GLOBAL libb2)

if(BLAKE2_PC_FOUND)
    if(NOT TARGET libb2::libb2)
        add_library(libb2::libb2 ALIAS PkgConfig::BLAKE2_PC)
    endif()

    set(BLAKE2_LIBRARY libb2::libb2 CACHE STRING "liblibb2 target")
    set(BLAKE2_COMPILE_OPTIONS ${BLAKE2_PC_CFLAGS_OTHER} CACHE STRING "liblibb2 target compile options")
    set(BLAKE2_VERSION ${BLAKE2_PC_VERSION} CACHE STRING "liblibb2 target version")

    find_path(BLAKE2_INCLUDE_DIR
        NAMES blake2.h
        HINTS ${BLAKE2_PC_INCLUDEDIR} ${BLAKE2_PC_INCLUDE_DIRS}
    )
    find_library(BLAKE2_LIBRARY
        NAMES ${BLAKE2_NAMES} b2 libb2
        HINTS ${BLAKE2_PC_LIBDIR} ${BLAKE2_PC_LIBRARY_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(
        BLAKE2
        REQUIRED_VARS BLAKE2_LIBRARY BLAKE2_INCLUDE_DIR
        VERSION_VAR BLAKE2_VERSION
    )
    set(BLAKE2_LIBRARIES ${BLAKE2_LIBRARY})
    set(BLAKE2_INCLUDE_DIRS ${BLAKE2_INCLUDE_DIR})

else()
    message(STATUS "BLAKE2: liblibb2 was not found via find_package() via FindBLAKE2.cmake module via PkgConfig.")
endif()

mark_as_advanced(
    BLAKE2_LIBRARY
    BLAKE2_INCLUDE_DIR
)
