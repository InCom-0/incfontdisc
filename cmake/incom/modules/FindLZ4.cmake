#[=======================================================================[.rst:
FindLZ4
--------------

Find LZ4 headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``lz4::lz4``
  The liblz4 library, if found.


Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project.
Note that one typically does not need to use these variables because lz4::lz4 is first class CMake target that encapsulates all of them.

``LZ4_FOUND``
  true if (the requested version of) LZ4 is available.
``LZ4_VERSION``
  the version of LZ4.
``LZ4_LIBRARY``
  the libraries to link against to use LZ4.
``LZ4_INCLUDE_DIR``
  where to find the LZ headers.
``LZ4_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(LZ4_PC QUIET IMPORTED_TARGET GLOBAL liblz4)

if(LZ4_PC_FOUND)
    if(NOT TARGET lz4::lz4)
        add_library(lz4::lz4 ALIAS PkgConfig::LZ4_PC)
    endif()

    set(LZ4_LIBRARY lz4::lz4 CACHE STRING "liblz4 target")
    set(LZ4_COMPILE_OPTIONS ${LZ4_PC_CFLAGS_OTHER} CACHE STRING "liblz4 target compile options")
    set(LZ4_VERSION ${LZ4_PC_VERSION} CACHE STRING "liblz4 target version")

    find_path(LZ4_INCLUDE_DIR
        NAMES lz4.h
        HINTS ${LZ4_PC_INCLUDEDIR} ${LZ4_PC_INCLUDE_DIRS}
    )
    find_library(LZ4_LIBRARY
        NAMES ${LZ4_NAMES} lz4
        HINTS ${LZ4_PC_LIBDIR} ${LZ4_PC_LIBRARY_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(
        LZ4
        REQUIRED_VARS LZ4_LIBRARY LZ4_INCLUDE_DIR
        VERSION_VAR LZ4_VERSION
    )
    set(LZ4_LIBRARIES ${LZ4_LIBRARY})
    set(LZ4_INCLUDE_DIRS ${LZ4_INCLUDE_DIR})

else()
    message(STATUS "LZ4: liblz4 was not found via find_package() via FindLZ4.cmake module via PkgConfig.")
endif()

mark_as_advanced(
    LZ4_LIBRARY
    LZ4_INCLUDE_DIR
)
