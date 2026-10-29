// OTS_LITE stand-in for GNU cgicc's HTMLDoctype.h
#ifndef _cgicc_HTMLDoctype_h_
#define _cgicc_HTMLDoctype_h_

#include <ostream>
#include <string>

namespace cgicc
{
class HTMLDoctype
{
  public:
	enum EDocumentType
	{
		eStrict,
		eTransitional,
		eFrames
	};
	explicit HTMLDoctype(EDocumentType = eStrict) {}
	void render(std::ostream& out) const { out << "<!DOCTYPE html>"; }
};
inline std::ostream& operator<<(std::ostream& out, const HTMLDoctype& d)
{
	d.render(out);
	return out;
}
}  // namespace cgicc

#endif
