include(BundleUtilities)

if (NOT DEFINED APP_BUNDLE OR APP_BUNDLE STREQUAL "")
    message(FATAL_ERROR "APP_BUNDLE is required")
endif()

if (NOT EXISTS "${APP_BUNDLE}")
    message(FATAL_ERROR "Bundle does not exist: ${APP_BUNDLE}")
endif()

if (DEFINED SEARCH_DIRS AND NOT SEARCH_DIRS STREQUAL "")
    string(REPLACE "|" ";" SEARCH_DIRS "${SEARCH_DIRS}")
else()
    set(SEARCH_DIRS "")
endif()

if (DEFINED EXTRA_LIBS AND NOT EXTRA_LIBS STREQUAL "")
    string(REPLACE "|" ";" EXTRA_LIBS "${EXTRA_LIBS}")
else()
    set(EXTRA_LIBS "")
endif()

set(valid_search_dirs "")
foreach(search_dir IN LISTS SEARCH_DIRS)
    if (EXISTS "${search_dir}")
        list(APPEND valid_search_dirs "${search_dir}")
    endif()
endforeach()

set(valid_extra_libs "")
set(bundle_frameworks_dir "${APP_BUNDLE}/Contents/Frameworks")
file(MAKE_DIRECTORY "${bundle_frameworks_dir}")

foreach(extra_lib IN LISTS EXTRA_LIBS)
    if (EXISTS "${extra_lib}")
        get_filename_component(extra_lib_name "${extra_lib}" NAME)
        set(bundled_extra_lib "${bundle_frameworks_dir}/${extra_lib_name}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${extra_lib}" "${bundled_extra_lib}"
        )
        list(APPEND valid_extra_libs "${bundled_extra_lib}")
    endif()
endforeach()

fixup_bundle("${APP_BUNDLE}" "${valid_extra_libs}" "${valid_search_dirs}")
