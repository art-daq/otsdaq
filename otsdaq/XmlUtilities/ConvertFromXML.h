#ifndef ots_ConvertFromXML_h
#define ots_ConvertFromXML_h

#include <string>
#include <xercesc/util/XMLChar.hpp>

namespace ots
{
class ConvertFromXML
{
  public:
	ConvertFromXML(const XMLCh* const toTranscode);
	~ConvertFromXML();

	const char* toConstChar(void) const;
	char*       toChar(void) const;
	std::string toString(void) const;

  private:
	char* xmlTranscoded_;
};

#define XML_TO_CONST_CHAR(xml) ConvertFromXML(xml).toConstChar()
#define XML_TO_CHAR(xml) ConvertFromXML(xml).toChar()
#define XML_TO_STRING(xml) ConvertFromXML(xml).toString()

}  // namespace ots

#endif
