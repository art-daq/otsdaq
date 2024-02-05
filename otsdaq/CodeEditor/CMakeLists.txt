cet_make_library(LIBRARY_NAME CodeEditor
SOURCE
CodeEditor.cc
CodeEditorSupervisor.cc
version.cc
		 LIBRARIES PRIVATE
		 otsdaq::CoreSupervisors
		 otsdaq::CgiDataUtilities
)


install_headers()
install_source()
