cet_register_export_set(SET_NAME IteratorPluginTypes NAMESPACE otsdaq_plugin_types)
cet_register_export_set(SET_NAME IteratorPluginSupport NAMESPACE otsdaq_plugin_support)

# Header-only contract between the Iterator and experiment Iterator plugins
cet_make_library(LIBRARY_NAME IteratorPluginInterface INTERFACE
		EXPORT_SET IteratorPluginTypes
		SOURCE IteratorPluginVInterface.h
		 LIBRARIES INTERFACE
		 otsdaq::Macros
		 XDAQ::xoap     # xoap::MessageReference in the interface
		 XercesC::XercesC
)

cet_make_library(LIBRARY_NAME iteratorPluginMaker
  EXPORT_SET IteratorPluginSupport
  SOURCE MakeIteratorPlugin.cc
  LIBRARIES
  PUBLIC
  otsdaq_plugin_types::IteratorPluginInterface
  PRIVATE
  fhiclcpp::fhiclcpp
  cetlib::cetlib
)

# keep GatewaySupervisor in the package default export set it has always used
cet_register_export_set(SET_NAME feCoreUsers SET_DEFAULT)
cet_make_library(LIBRARY_NAME GatewaySupervisor
SOURCE
GatewaySupervisor.cc
Iterator.cc
IteratorPluginVInterface.cc
PixelHistoPicGen.cc
version.cc
		 LIBRARIES PUBLIC
		 otsdaq_plugin_types::IteratorPluginInterface
		 PRIVATE
		 otsdaq::CoreSupervisors
		 otsdaq::IterateTable
		 otsdaq::CgiDataUtilities
		 otsdaq::WorkLoopManager
		 otsdaq::CodeEditor
		 otsdaq::SupervisorInfo
		 otsdaq::NetworkUtilities
		 otsdaq_plugin_support::runInfoMaker
		 otsdaq_plugin_support::iteratorPluginMaker
)

# Plugin type for experiment packages: cet_build_plugin(<Name> otsdaq::iteratorPlugin ...)
#	produces lib<Name>_iteratorPlugin.so, found by makeIteratorPlugin("<Name>")
include(BasicPlugin)
cet_write_plugin_builder(otsdaq::iteratorPlugin art::plugin Modules
SUFFIX iteratorPlugin INSTALL_BUILDER
LIBRARIES CONDITIONAL otsdaq::GatewaySupervisor)

install_headers()
install_source()
