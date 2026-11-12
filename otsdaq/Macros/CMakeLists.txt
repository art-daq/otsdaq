
if(DEFINED ENV{OTS_MAXLITE})
	# maxlite: TRACE-free logging (OtsLog) instead of TRACE::MF
	# and dlopen plugin loading (PluginFactory) instead of cetlib
	cet_make_library(LIBRARY_NAME Macros
	SOURCE BinaryStringMacros.cc StringMacros.cc OtsLog.cc PluginFactory.cc
			 LIBRARIES PUBLIC
			 Threads::Threads
			 ${CMAKE_DL_LIBS}
			)
	# every target using otsdaq headers (also in other repos, through the exported
	# targets) must compile with the same maxlite branches of CoutMacros.h etc.
	target_compile_definitions(Macros PUBLIC OTS_LITE OTS_MAXLITE)
else()
cet_make_library(LIBRARY_NAME Macros
SOURCE BinaryStringMacros.cc StringMacros.cc
		 LIBRARIES PUBLIC
		 TRACE::MF
		)
endif()

install_headers()
install_source()
