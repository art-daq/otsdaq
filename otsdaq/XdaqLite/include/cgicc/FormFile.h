// OTS_LITE stand-in for GNU cgicc's FormFile (multipart upload)
#ifndef _cgicc_FormFile_h_
#define _cgicc_FormFile_h_

#include <ostream>
#include <string>

namespace cgicc
{
class FormFile
{
  public:
	FormFile() {}
	FormFile(const std::string& name, const std::string& filename, const std::string& dataType, const std::string& data)
	    : name_(name)
	    , filename_(filename)
	    , dataType_(dataType)
	    , data_(data) {}

	std::string   getName() const { return name_; }
	std::string   getFilename() const { return filename_; }
	std::string   getDataType() const { return dataType_; }
	std::string   getData() const { return data_; }
	unsigned long getDataLength() const { return data_.size(); }
	void          writeToStream(std::ostream& out) const { out << data_; }

  private:
	std::string name_, filename_, dataType_, data_;
};
}  // namespace cgicc

#endif
