#ifndef OTSDAQ_OTSDAQ_XMLUTILITIES_DOMTREEERRORREPORTER_H_
#define OTSDAQ_OTSDAQ_XMLUTILITIES_DOMTREEERRORREPORTER_H_

#include <string>
#include <xercesc/sax/ErrorHandler.hpp>
#include <xercesc/sax/SAXParseException.hpp>

namespace ots
{
class DOMTreeErrorReporter : public xercesc::ErrorHandler
{
  public:
	DOMTreeErrorReporter();
	~DOMTreeErrorReporter();

	void warning(const xercesc::SAXParseException& exception);
	void error(const xercesc::SAXParseException& exception);
	void fatalError(const xercesc::SAXParseException& exception);
	void resetErrors(void);

  private:
	std::string reportParseException(const xercesc::SAXParseException& exception);
};

}  // namespace ots
#endif  // OTSDAQ_OTSDAQ_XMLUTILITIES_DOMTREEERRORREPORTER_H_
