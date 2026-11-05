# OTS_LITE stand-in for xdaq and for the few artdaq utilities otsdaq core uses.
# Built only when OTS_LITE is set (see otsdaq/CMakeLists.txt). The include/ tree mirrors
# the xdaq and artdaq header paths so sources compile unchanged; the XDAQ::* targets the
# rest of otsdaq links are aliased to this one library.

cet_make_library(LIBRARY_NAME XdaqLite
	SOURCE XdaqLite.cc ArtdaqLite.cc
	LIBRARIES PUBLIC
	Threads::Threads
	fhiclcpp::fhiclcpp
	cetlib::cetlib
	messagefacility::MF_MessageLogger
)
target_include_directories(XdaqLite PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include/otsdaq/XdaqLite/include>)

foreach(lib xdaq xoap xgi xcept xdata toolbox cgicc config peer pthttp)
	add_library(XDAQ::${lib} ALIAS XdaqLite)
endforeach()

# The lite executive: drop-in for xdaq.exe (same -p/-h/-e/-c command line the `ots` launcher uses).
cet_make_exec(NAME ots_lite_main
	SOURCE ots_lite_main.cc
	LIBRARIES PRIVATE
	otsdaq::XdaqLite
	XercesC::XercesC
	Threads::Threads
	${CMAKE_DL_LIBS}
)
# `ots` execs xdaq.exe by name; point it at the lite executive. CMAKE_INSTALL_PREFIX is left
# unexpanded so `cmake --install --prefix <other>` relocates the link along with the binary.
install(CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink ots_lite_main \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin/xdaq.exe\")")

# MessageFacility destinations named in otsdaq's MessageFacility*.fcl files; the originals
# are in artdaq_mfextensions, which lite does not build. The library names must match the
# ones MessageFacility looks up: libOTS_mfPlugin.so and libUDP_mfPlugin.so.
include(mfPlugin)
cet_build_plugin(OTS mfPlugin LIBRARIES REG messagefacility::MF_MessageLogger)
cet_build_plugin(UDP mfPlugin LIBRARIES REG messagefacility::MF_MessageLogger)

install(DIRECTORY include DESTINATION include/otsdaq/XdaqLite)
install_headers()
install_source()
