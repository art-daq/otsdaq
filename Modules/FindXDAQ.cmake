#[================================================================[.rst:
FindXDAQ
----------
  find XDAQ

#]================================================================]

# See if it was already found and is in cache
if (XDAQ_FIND_REQUIRED)
  set(_cet_XDAQ_FIND_REQUIRED ${XDAQ_FIND_REQUIRED})
  unset(XDAQ_FIND_REQUIRED)
else()
  unset(_cet_XDAQ_FIND_REQUIRED)
endif()
find_package(XDAQ CONFIG QUIET)
if (_cet_XDAQ_FIND_REQUIRED)
  set(XDAQ_FIND_REQUIRED ${_cet_XDAQ_FIND_REQUIRED})
  unset(_cet_XDAQ_FIND_REQUIRED)
endif()

if (XDAQ_FOUND)
    set(_cet_XDAQ_config_mode CONFIG_MODE)
else(XDAQ_FOUND)
    unset(_cet_XDAQ_config_mode)
    find_file(_xdaq_h NAMES xdaq/Application.h HINTS ENV XDAQ_ROOT PATH_SUFFIXES include)
    if (_xdaq_h)
        #message("Found xdaq/Application.h: ${_xdaq_h}")
        string(REPLACE "xdaq/Application.h" "" _cet_XDAQ_include_dir "${_xdaq_h}")
        if (_cet_XDAQ_include_dir STREQUAL "/")
        unset(_cet_XDAQ_include_dir)
        endif()
    endif()

    if (EXISTS "${_cet_XDAQ_include_dir}")
        set(XDAQ_FOUND TRUE)
        get_filename_component(_cet_XDAQ_dir "${_cet_XDAQ_include_dir}" PATH)
        if (_cet_XDAQ_dir STREQUAL "/")
        unset(_cet_XDAQ_dir)
        endif()
        set(XDAQ_INCLUDE_DIRS "${_cet_XDAQ_include_dir};${_cet_XDAQ_include_dir}/linux")
        set(XDAQ_LIBRARY_DIR "${_cet_XDAQ_dir}/lib")
        find_library( XDAQ_CGICC_LIBRARY NAMES cgicc PATHS ${XDAQ_LIBRARY_DIR} REQUIRED)
        find_library( XDAQ_CONFIG_LIBRARY NAMES config PATHS ${XDAQ_LIBRARY_DIR} REQUIRED)
        find_library( XDAQ_PEER_LIBRARY NAMES peer PATHS ${XDAQ_LIBRARY_DIR} REQUIRED)
        find_library( XDAQ_PTHTTP_LIBRARY NAMES pthttp PATHS ${XDAQ_LIBRARY_DIR} REQUIRED)
        find_library( XDAQ_TOOLBOX_LIBRARY NAMES toolbox PATHS ${XDAQ_LIBRARY_DIR} REQUIRED)
        find_library( XDAQ_XCEPT_LIBRARY NAMES xcept PATHS ${XDAQ_LIBRARY_DIR} REQUIRED)
        find_library( XDAQ_XDAQ_LIBRARY NAMES xdaq PATHS ${XDAQ_LIBRARY_DIR} REQUIRED)
        find_library( XDAQ_XDATA_LIBRARY NAMES xdata PATHS ${XDAQ_LIBRARY_DIR} REQUIRED)
        find_library( XDAQ_XGI_LIBRARY NAMES xgi PATHS ${XDAQ_LIBRARY_DIR} REQUIRED)
        find_library( XDAQ_XOAP_LIBRARY NAMES xoap PATHS ${XDAQ_LIBRARY_DIR} REQUIRED)
    endif()
endif(XDAQ_FOUND)

if(XDAQ_FOUND)
  if (NOT TARGET XDAQ::cgicc)
    add_library(XDAQ::cgicc SHARED IMPORTED)
    set_target_properties(XDAQ::cgicc PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${XDAQ_INCLUDE_DIRS}" IMPORTED_LOCATION "${XDAQ_CGICC_LIBRARY}")
  endif()
  if (NOT TARGET XDAQ::config)
    add_library(XDAQ::config SHARED IMPORTED)
    set_target_properties(XDAQ::config PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${XDAQ_INCLUDE_DIRS}" IMPORTED_NO_SONAME TRUE IMPORTED_LOCATION "${XDAQ_CONFIG_LIBRARY}")
  endif()
  if (NOT TARGET XDAQ::peer)
    add_library(XDAQ::peer SHARED IMPORTED)
    set_target_properties(XDAQ::peer PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${XDAQ_INCLUDE_DIRS}" IMPORTED_NO_SONAME TRUE IMPORTED_LOCATION "${XDAQ_PEER_LIBRARY}")
  endif()
  if (NOT TARGET XDAQ::pthttp)
    add_library(XDAQ::pthttp SHARED IMPORTED)
    set_target_properties(XDAQ::pthttp PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${XDAQ_INCLUDE_DIRS}" IMPORTED_NO_SONAME TRUE IMPORTED_LOCATION "${XDAQ_PTHTTP_LIBRARY}" INTERFACE_LINK_LIBRARIES XDAQ::xgi)
  endif()
  if (NOT TARGET XDAQ::toolbox)
    add_library(XDAQ::toolbox SHARED IMPORTED)
    set_target_properties(XDAQ::toolbox PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${XDAQ_INCLUDE_DIRS}" IMPORTED_NO_SONAME TRUE IMPORTED_LOCATION "${XDAQ_TOOLBOX_LIBRARY}")
  endif()
  if (NOT TARGET XDAQ::xcept)
    add_library(XDAQ::xcept SHARED IMPORTED)
    set_target_properties(XDAQ::xcept PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${XDAQ_INCLUDE_DIRS}" IMPORTED_NO_SONAME TRUE IMPORTED_LOCATION "${XDAQ_XCEPT_LIBRARY}")
  endif()
  if (NOT TARGET XDAQ::xdata)
    add_library(XDAQ::xdata SHARED IMPORTED)
    set_target_properties(XDAQ::xdata PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${XDAQ_INCLUDE_DIRS}" IMPORTED_NO_SONAME TRUE IMPORTED_LOCATION "${XDAQ_XDATA_LIBRARY}")
  endif()
  if (NOT TARGET XDAQ::xdaq)
    add_library(XDAQ::xdaq SHARED IMPORTED)
    set_target_properties(XDAQ::xdaq PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${XDAQ_INCLUDE_DIRS}"
        IMPORTED_NO_SONAME TRUE
        IMPORTED_LOCATION "${XDAQ_XDAQ_LIBRARY}"
        INTERFACE_LINK_LIBRARIES "XDAQ::xdata;XDAQ::peer"
    )
  endif()
  if (NOT TARGET XDAQ::xgi)
    add_library(XDAQ::xgi SHARED IMPORTED)
    set_target_properties(XDAQ::xgi PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${XDAQ_INCLUDE_DIRS}" IMPORTED_NO_SONAME TRUE IMPORTED_LOCATION "${XDAQ_XGI_LIBRARY}")
  endif()
  if (NOT TARGET XDAQ::xoap)
    add_library(XDAQ::xoap SHARED IMPORTED)
    set_target_properties(XDAQ::xoap PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${XDAQ_INCLUDE_DIRS}" IMPORTED_NO_SONAME TRUE IMPORTED_LOCATION "${XDAQ_XOAP_LIBRARY}")
  endif()
endif(XDAQ_FOUND)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(XDAQ ${_cet_XDAQ_config_mode}
  REQUIRED_VARS XDAQ_FOUND
  XDAQ_INCLUDE_DIRS
  XDAQ_LIBRARY_DIR
)

unset(_cet_XDAQ_FIND_REQUIRED)
unset(_cet_XDAQ_config_mode)
unset(_cet_XDAQ_dir)
unset(_cet_XDAQ_include_dir)
