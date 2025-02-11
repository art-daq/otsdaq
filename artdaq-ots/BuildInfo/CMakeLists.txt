
# JCF, 1/14/15

# The "include_directories()" macro below is not strictly necessary
# from an artdaq-demo perspective, but if users of other packages wish
# to implement their own BuildInfo-style modules they can copy this
# CMakeLists.txt file and easily use "include_directories()" to set
# their include path(s)

# PROJECT_SOURCE_DIR contains the full path to the root of your
# project source directory, i.e. to the nearest directory where
# CMakeLists.txt contains the PROJECT() command

# Uncomment the MESSAGE() macro to actually see what
# PROJECT_SOURCE_DIR is set to during the build

# MESSAGE( STATUS "PROJECT_SOURCE_DIR:         " ${PROJECT_SOURCE_DIR} )

include_directories( ${PROJECT_SOURCE_DIR} )

# the utcDateTime variable is used by the GetPackageBuildInfo code
# to report the build datetime
execute_process( COMMAND "date" "-u" "+%d-%b-%Y %H:%M:%S %Z" OUTPUT_VARIABLE rawUTCDate )
string( STRIP ${rawUTCDate} utcDateTime )

execute_process(COMMAND git describe --tags
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
				RESULT_VARIABLE GIT_DESCRIBE_RESULT 
                OUTPUT_VARIABLE GIT_DESCRIBE_VERSION 
                ERROR_QUIET
				OUTPUT_STRIP_TRAILING_WHITESPACE)

message("${GIT_DESCRIBE_RESULT}: ${GIT_DESCRIBE_VERSION} || $ENV{CETPKG_VERSION}")

if(${GIT_DESCRIBE_RESULT} EQUAL 0)
	set(version_with_git ${GIT_DESCRIBE_VERSION})
else(${GIT_DESCRIBE_RESULT})
	set(version_with_git $ENV{CETPKG_VERSION})
endif()

configure_file ( ${CMAKE_CURRENT_SOURCE_DIR}/GetPackageBuildInfo.cc.in
  ${CMAKE_CURRENT_BINARY_DIR}/GetPackageBuildInfo.cc @ONLY )

cet_make_library( 
                  SOURCE
		  ${CMAKE_CURRENT_BINARY_DIR}/GetPackageBuildInfo.cc
			LIBRARIES artdaq_core::artdaq-core_Data
                 )

install_headers()
install_source()
