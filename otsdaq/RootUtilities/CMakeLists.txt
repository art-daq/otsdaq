cet_make_library(LIBRARY_NAME RootUtilities
    SOURCE
    DQMHistosBase.cc
    RootFileExplorer.cpp
    VisualDataManager.cc
	      LIBRARIES PUBLIC
          ROOT::Core
          ROOT::RIO
          XercesC::XercesC
          otsdaq::DataManager
          PRIVATE
          otsdaq::TableCore
          otsdaq::ConfigurationInterface
)

install_headers()
install_source()
