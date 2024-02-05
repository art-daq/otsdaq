cet_make_library(LIBRARY_NAME XDAQContextTable SOURCE XDAQContextTable.cc
LIBRARIES
PUBLIC
otsdaq::ConfigurationInterface
PRIVATE

)

cet_build_plugin(XDAQContextTable otsdaq::table LIBRARIES REG otsdaq::XDAQContextTable )


install_headers()
install_source()
