include_guard(GLOBAL)

find_package(FLTK REQUIRED)

if(NOT TARGET FLTK::FLTK)
    add_library(FLTK::FLTK INTERFACE IMPORTED)

    # FLTK modern (vcpkg) mengekspos FLTK_INCLUDE_DIRS, tapi
    # FindFLTK.cmake versi lama kadang hanya set FLTK_INCLUDE_DIR.
    # Gabungkan semuanya + fallback ke path vcpkg standar.
    set(_fltk_includes "")
    if(DEFINED FLTK_INCLUDE_DIRS)
        list(APPEND _fltk_includes ${FLTK_INCLUDE_DIRS})
    endif()
    if(DEFINED FLTK_INCLUDE_DIR)
        list(APPEND _fltk_includes ${FLTK_INCLUDE_DIR})
    endif()

    # Fallback: vcpkg include dir (tempat FL/Fl.H berada).
    if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
        list(APPEND _fltk_includes
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
    endif()

    list(REMOVE_DUPLICATES _fltk_includes)

    target_include_directories(FLTK::FLTK INTERFACE ${_fltk_includes})
    target_link_libraries(FLTK::FLTK INTERFACE ${FLTK_LIBRARIES})

    message(STATUS "[frans] FLTK includes: ${_fltk_includes}")
    message(STATUS "[frans] FLTK libs: ${FLTK_LIBRARIES}")
endif()