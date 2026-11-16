cet_make_library(LIBRARY_NAME XDAQContextTable SOURCE XDAQContextTableImpl.cc
LIBRARIES
PUBLIC
otsdaq::ConfigurationInterface
PRIVATE

)

# The source is named ...Impl.cc (like DesktopIconTable, IterateTable) so cet_build_plugin does
# not compile it a second time into an implementation library: two copies of the static
# AppClassModuleLookup_ were destroyed twice at exit ("double free", seen in ots maxlite).
cet_build_plugin(XDAQContextTable otsdaq::table LIBRARIES REG otsdaq::XDAQContextTable )


install_headers()
install_source()
