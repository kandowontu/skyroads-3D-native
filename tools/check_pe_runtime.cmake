if(NOT DEFINED EXECUTABLE OR NOT EXISTS "${EXECUTABLE}")
    message(FATAL_ERROR "Native executable does not exist: ${EXECUTABLE}")
endif()
if(NOT DEFINED PE_INSPECTOR OR NOT EXISTS "${PE_INSPECTOR}")
    message(FATAL_ERROR "PE dependency inspector does not exist: ${PE_INSPECTOR}")
endif()

if(PE_INSPECTOR_MODE STREQUAL "objdump")
    set(inspector_arguments -p "${EXECUTABLE}")
elseif(PE_INSPECTOR_MODE STREQUAL "dumpbin")
    set(inspector_arguments /DEPENDENTS "${EXECUTABLE}")
elseif(PE_INSPECTOR_MODE STREQUAL "link")
    set(inspector_arguments /DUMP /DEPENDENTS "${EXECUTABLE}")
else()
    message(FATAL_ERROR "Unknown PE inspector mode: ${PE_INSPECTOR_MODE}")
endif()

execute_process(
    COMMAND "${PE_INSPECTOR}" ${inspector_arguments}
    RESULT_VARIABLE inspector_result
    OUTPUT_VARIABLE dependency_output
    ERROR_VARIABLE inspector_error)
if(NOT inspector_result EQUAL 0)
    message(FATAL_ERROR
        "Could not inspect ${EXECUTABLE}: ${inspector_error}")
endif()

string(TOLOWER "${dependency_output}" dependency_output_lower)
set(forbidden_runtimes
    "libgcc"
    "libstdc++"
    "libwinpthread"
    "msvcp"
    "vcruntime"
    "concrt")
foreach(runtime IN LISTS forbidden_runtimes)
    string(FIND "${dependency_output_lower}" "${runtime}" runtime_position)
    if(NOT runtime_position EQUAL -1)
        message(FATAL_ERROR
            "Release executable requires external compiler runtime ${runtime}")
    endif()
endforeach()

string(REGEX MATCHALL "[A-Za-z0-9_.+-]+\\.[Dd][Ll][Ll]"
    imported_dlls "${dependency_output}")
list(REMOVE_DUPLICATES imported_dlls)
list(SORT imported_dlls)

if(STRICT_WINDOWS_ONLY)
    set(allowed_windows_dlls
        "gdi32.dll"
        "kernel32.dll"
        "user32.dll"
        "winmm.dll")
    foreach(imported_dll IN LISTS imported_dlls)
        string(TOLOWER "${imported_dll}" imported_dll_lower)
        list(FIND allowed_windows_dlls "${imported_dll_lower}" allowed_index)
        if(allowed_index EQUAL -1)
            message(FATAL_ERROR
                "Release executable imports non-system DLL ${imported_dll}")
        endif()
    endforeach()
endif()

message(STATUS "Imported Windows DLLs: ${imported_dlls}")
