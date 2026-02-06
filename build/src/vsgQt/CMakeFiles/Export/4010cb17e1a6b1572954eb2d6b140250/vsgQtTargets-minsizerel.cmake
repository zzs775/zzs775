#----------------------------------------------------------------
# Generated CMake target import file for configuration "MinSizeRel".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "vsgQt::vsgQt" for configuration "MinSizeRel"
set_property(TARGET vsgQt::vsgQt APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(vsgQt::vsgQt PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_MINSIZEREL "CXX"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/lib/vsgQts.lib"
  )

list(APPEND _cmake_import_check_targets vsgQt::vsgQt )
list(APPEND _cmake_import_check_files_for_vsgQt::vsgQt "${_IMPORT_PREFIX}/lib/vsgQts.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
