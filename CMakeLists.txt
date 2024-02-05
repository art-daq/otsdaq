# ======================================================================
#  otsdaq main build file
#
#  cd .../path/to/build/directory
#  source .../path/to/otsdaq/ups/setup_for_development
#  buildtool
# ======================================================================


# use cmake 3.17 or later
cmake_minimum_required (VERSION 3.19 FATAL_ERROR)

find_package(cetmodules 3.16.00 REQUIRED)

project(otsdaq VERSION 2.07.00)

include(CetCMakeEnv)
cet_cmake_env()

# Find our own CMake modules, and tell everyone else where to find them.
cet_cmake_module_directories(Modules BINARY)

if (CMAKE_CXX_COMPILER_ID STREQUAL Clang AND
     CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14.0.0)
  # Clang 14 has a bug in which it complains about destructors of template classes
  # e.g. ArtdaqInput<T>::~ArtdaqInput(), when it is expecting the destructor name to
  # match what is on the left-hand side of the ::. This is a violation, however, of
  # the C++20 standard. https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p1787r6.html
  set(DT_FLAG -Wno-dtor-name)
else()
  set(DT_FLAG)
endif()

cet_set_compiler_flags(
 DIAGS 
 VIGILANT
 WERROR
 NO_UNDEFINED
 EXTRA_FLAGS -pedantic -D_LIBCPP_ENABLE_CXX17_REMOVED_FEATURES=1 ${DT_FLAG}
 )
  
# Debug streamer.
string(TOUPPER ${CMAKE_BUILD_TYPE} BTYPE_UC)
if(BTYPE_UC STREQUAL DEBUG)
  add_definitions(-DDEBUGME)
	if(DEFINED ENV{USE_ASAN})
		if($ENV{USE_ASAN} GREATER 0)
			message("!!! Address Sanitizer Activated!!!")
			add_compile_options(-fsanitize=address -Wno-unused-command-line-argument)
			add_link_options(-lasan)
		endif()
	endif()
	if(DEFINED ENV{USE_TSAN})
		if($ENV{USE_TSAN} GREATER 0)
			message("!!! Thread Sanitizer Activated!!!")
			add_compile_options(-fsanitize=thread -Wno-unused-command-line-argument -Wno-tsan)
			add_link_options(-ltsan)
		endif()
	endif()
	if(DEFINED ENV{USE_UBSAN})
		if($ENV{USE_UBSAN} GREATER 0)
			message("!!! Undefined Behavior Sanitizer Activated!!!")
			add_compile_options(-fsanitize=undefined -Wno-unused-command-line-argument)
			add_link_options(-lubsan)
		endif()
	endif()
	if(DEFINED ENV{USE_GCOV})
		add_compile_options(-fprofile-arcs -ftest-coverage --coverage)
		add_link_options(--coverage)
	endif()
endif()

find_package(artdaq_core 3.09.00 REQUIRED EXPORT)
find_package(artdaq 3.12.00 REQUIRED EXPORT)
find_package(artdaq_database 1.07.00 REQUIRED)
find_package(Python 3.6 COMPONENTS Development.Embed REQUIRED)

if (EXISTS ${artdaq_database_DIR})
set(bsoncxx_DIR ${artdaq_database_DIR})
set(mongocxx_DIR ${artdaq_database_DIR})
find_package(mongocxx REQUIRED)
endif()
find_package(Boost QUIET COMPONENTS date_time program_options regex thread REQUIRED EXPORT)

 # XDAQ Extra setup
 include_directories($ENV{XDAQ_INC}/linux $ENV{XDAQ_INC})
link_directories($ENV{XDAQ_LIB})
 set(XDAQ_BASIC_LIB_LIST 
   xcept
   xoap
   xdaq
   toolbox
   xerces-c
   cgicc
   Threads::Threads
 )
 set(XDAQ_XERCES-C xerces-c)

#string(REGEX MATCHALL "[0-9][0-9]?[0-9]?" ART_VERSION_LIST "$ENV{ART_VERSION}")
#list(GET ART_VERSION_LIST 0 ART_MAJOR_VERSION)
#list(GET ART_VERSION_LIST 1 ART_MINOR_VERSION)
#list(GET ART_VERSION_LIST 2 ART_PATCH_VERSION)

#message("-DART_HEX_VERSION=0x${ART_MAJOR_VERSION}${ART_MINOR_VERSION}${ART_PATCH_VERSION}")
#set(ART_HEX_VERSION 0x${ART_MAJOR_VERSION}${ART_MINOR_VERSION}${ART_PATCH_VERSION})
#add_definitions(-DART_HEX_VERSION=0x${ART_MAJOR_VERSION}${ART_MINOR_VERSION}${ART_PATCH_VERSION})

#string(REGEX MATCHALL "[0-9][0-9]?[0-9]?" MESSAGEFACILITY_VERSION_LIST "$ENV{MESSAGEFACILITY_VERSION}")
#list(GET MESSAGEFACILITY_VERSION_LIST 0 MESSAGEFACILITY_MAJOR_VERSION)
#list(GET MESSAGEFACILITY_VERSION_LIST 1 MESSAGEFACILITY_MINOR_VERSION)
#list(GET MESSAGEFACILITY_VERSION_LIST 2 MESSAGEFACILITY_PATCH_VERSION)

# as of fhiclcpp v4_15_00 (s108), ParameterSet::make is the preferred way to make a ParameterSet
#check_ups_version(fhiclcpp $ENV{FHICLCPP_VERSION} v4_15_00 PRODUCT_MATCHES_VAR PSET_MAKE)
#if(PSET_MAKE)
#  add_definitions(-DSIMPLER_PSET_MAKE)
#endif()

#message("-DMESSAGEFACILITY_HEX_VERSION=0x${MESSAGEFACILITY_MAJOR_VERSION}${MESSAGEFACILITY_MINOR_VERSION}${MESSAGEFACILITY_PATCH_VERSION}")
#add_definitions(-DMESSAGEFACILITY_HEX_VERSION=0x${MESSAGEFACILITY_MAJOR_VERSION}${MESSAGEFACILITY_MINOR_VERSION}${MESSAGEFACILITY_PATCH_VERSION})

# For version.h
message("otsdaq PROJECT_VERSION ${PROJECT_VERSION}")
string(REGEX MATCHALL "[0-9][0-9]?" OTSDAQ_VERSION_LIST "${PROJECT_VERSION}")
list(GET OTSDAQ_VERSION_LIST 0 OTSDAQ_MAJOR_VERSION)
list(GET OTSDAQ_VERSION_LIST 1 OTSDAQ_MINOR_VERSION)
list(GET OTSDAQ_VERSION_LIST 2 OTSDAQ_PATCH_VERSION)
add_definitions(-DOTSDAQ_MAJOR_VERSION=${OTSDAQ_MAJOR_VERSION})
add_definitions(-DOTSDAQ_MINOR_VERSION=${OTSDAQ_MINOR_VERSION})
add_definitions(-DOTSDAQ_PATCH_VERSION=${OTSDAQ_PATCH_VERSION})

include(BuildPlugins)

# source
add_subdirectory(artdaq-ots)
cet_register_export_set(SET_NAME artdaqUsers SET_DEFAULT)
add_subdirectory(otsdaq)
cet_register_export_set(SET_NAME otsdaqUsers SET_DEFAULT)

# tools
add_subdirectory(tools)

# Documentation (to comment out if not needed DOXYGEN locally)
if( $ENV{OTS_DOXY} MATCHES "DOIT" )
	add_subdirectory(doc)
endif()

# test programs
add_subdirectory(test)


# data-core
FILE(COPY data-core DESTINATION .)
INSTALL(DIRECTORY data-core DESTINATION .)

cet_cmake_config()
