#cet_make(LIBRARY_NAME TablePlugins
#		 LIBRARIES
#		 TableCore
#         cetlib
#)

include(otsdaq::table)

# Note: ARTDAQTableBase and the ARTDAQ table plugins live in the otsdaq-artdaqinterface repository
add_subdirectory(SlowControlsTableBase)
add_subdirectory(XDAQContextTable)

cet_build_plugin(FESlowControlsTable otsdaq::table LIBRARIES REG otsdaq::SlowControlsTableBase )

cet_make_library(LIBRARY_NAME DesktopIconTable SOURCE DesktopIconTableImpl.cc LIBRARIES PRIVATE otsdaq::WebUsersUtilities otsdaq::XDAQContextTable)
cet_build_plugin(DesktopIconTable otsdaq::table LIBRARIES REG otsdaq::DesktopIconTable )

cet_build_plugin(MessageFacilityTable otsdaq::table  )

cet_make_library(LIBRARY_NAME IterateTable SOURCE IterateTableImpl.cc LIBRARIES PRIVATE otsdaq::ConfigurationInterface)
cet_build_plugin(IterateTable otsdaq::table LIBRARIES REG otsdaq::IterateTable )



install_headers()
install_source()
