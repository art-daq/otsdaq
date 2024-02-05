#ifndef ots_ConvertToXML_h
#define ots_ConvertToXML_h

#include <string>
#include <xercesc/util/XMLChar.hpp>

namespace ots
{
class ConvertToXML
{
  public:
	ConvertToXML(const char* const toTranscode);
	ConvertToXML(const std::string& toTranscode);
	ConvertToXML(const int toTranscode);
	~ConvertToXML(void);

	const XMLCh* unicodeForm() const;

  private:
	XMLCh* fUnicodeForm_;
};

#define CONVERT_TO_XML(str) ConvertToXML(str).unicodeForm()

}  // namespace ots

#endif
