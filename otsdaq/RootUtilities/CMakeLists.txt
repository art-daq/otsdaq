cet_make_library(LIBRARY_NAME RootUtilities
    SOURCE
    DQMHistosBase.cc
    RootFileExplorer.cpp
    VisualDataManager.cc
	      LIBRARIES PUBLIC
          ROOT::Core
          ROOT::RIO
          PRIVATE
          otsdaq::TableCore
          otsdaq::DataManager
          otsdaq::ConfigurationInterface
)

install_headers()
install_source()
