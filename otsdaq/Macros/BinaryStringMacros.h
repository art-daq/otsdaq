#ifndef _ots_BinaryStringMacros_h_
#define _ots_BinaryStringMacros_h_

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/StringMacros.h"

#include <map>
#include <set>
#include <typeinfo>  // operator typeid
#include <vector>

namespace ots
{
struct BinaryStringMacros
{
	// clang-format off
  private:  // private constructor because all static members, should never instantiate
	        // this class
	BinaryStringMacros(void);
	~BinaryStringMacros(void);

  public:
	// Here is the list of static helper functions:
	//
	//		binaryStringToHexString
	//		binaryNumberToHexString
	//


	//=======================================================
	static std::string 	binaryStringToHexString			(
		const void*        							binaryBuffer,
		unsigned int       							numberOfBytes,
		const std::string& 							resultPreamble  = "",
		const std::string& 							resultDelimiter = "");
		
	template<class T>
	static std::string 	binaryNumberToHexString		(
		const T& 									binaryBuffer,
		const std::string& 							resultPreamble  = "0x",
		const std::string& 							resultDelimiter = " ");
	static std::string 	binaryNumberToHexString		(
		const std::string& 							binaryBuffer,
		const std::string& 							resultPreamble  = "0x",
		const std::string& 							resultDelimiter = " ");
	static std::string 	binaryNumberToHexString		(
		const void*        							binaryBuffer,
		unsigned int       							numberOfBytes,
		const std::string& 							resultPreamble  = "0x",
		const std::string& 							resultDelimiter = " ");


	//=======================================================
	template<class T>
	static void 		insertValueInBinaryString	( // defined in included .icc source
		std::string& 								binaryBuffer, 
        T            								value,
        unsigned int 								bitIndex  = 0);
	
	// specialized for string value
	static void 		insertValueInBinaryString	( 
		std::string&       							binaryBuffer,
        const std::string& 							value,
        unsigned int       							bitIndex  = 0);

	//=======================================================
    template<class T>
	static void 		extractValueFromBinaryString( // defined in included .icc source
		const std::string&							binaryBuffer, 
        T&            								value,
        unsigned int 								bitIndex  = 0);
	
	// specialized for string value
	static void 		extractValueFromBinaryString( 
		const std::string&       					binaryBuffer,
        std::string& 								value,
		unsigned int       							valueNumberOfBits,
        unsigned int       							bitIndex  = 0);   

	// specialized for generic void* handling (actually does the work)
	static void 		extractValueFromBinaryString( 
		const void*       							binaryBuffer,
		unsigned int       							bufferNumberOfBytes,
        void* 										value,
		unsigned int       							valueNumberOfBits,
        unsigned int       							bitIndex  = 0);   

};  // end BinaryStringMacros static class

#include "otsdaq/Macros/BinaryStringMacros.icc"  //define template functions

// clang-format on
}  // namespace ots
#endif
