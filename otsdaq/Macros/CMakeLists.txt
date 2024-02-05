
cet_make_library(LIBRARY_NAME Macros
SOURCE BinaryStringMacros.cc StringMacros.cc
		 LIBRARIES PUBLIC
		 TRACE::MF
        )
        
install_headers()
install_source()
