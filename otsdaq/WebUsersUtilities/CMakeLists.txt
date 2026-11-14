if(DEFINED ENV{OTS_MAXLITE} OR (APPLE AND DEFINED ENV{OTS_LITE}))
	# SHA-512; Linux lite/full get libcrypto through cetlib, macOS cetlib uses CommonCrypto instead
	set(OTS_CRYPTO_LIBRARIES OpenSSL::Crypto)
endif()
cet_make_library(LIBRARY_NAME WebUsersUtilities
SOURCE RemoteWebUsers.cc
WebUsers.cc
		 LIBRARIES PUBLIC
		 otsdaq::MessageFacility
		 otsdaq::SOAPUtilities
		 otsdaq::TableCore
		 PRIVATE
		 otsdaq::XmlUtilities
		 otsdaq::NetworkUtilities
		 otsdaq::CgiDataUtilities
		 otsdaq::SupervisorInfo
		 ${OTS_CRYPTO_LIBRARIES}
)

if(DEFINED ENV{OTS_MAXLITE} OR (APPLE AND DEFINED ENV{OTS_LITE}))
	# SHA512_Init/Update/Final are deprecated in OpenSSL 3; lite gets this setting through cetlib
	target_compile_definitions(WebUsersUtilities PRIVATE OPENSSL_API_COMPAT=0x10100000L)
endif()

install_headers()
install_source()
