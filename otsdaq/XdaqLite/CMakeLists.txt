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

install(DIRECTORY include DESTINATION include/otsdaq/XdaqLite)
install_headers()
install_source()
