// OTS_LITE stand-in for GNU cgicc's Cgicc: parsed form/query data of one HTTP request.
// Surface limited to what otsdaq uses: getElement/getElements/getFiles/getEnvironment/form_urldecode.
#ifndef _cgicc_Cgicc_h_
#define _cgicc_Cgicc_h_

#include <string>
#include <vector>

#include "cgicc/CgiEnvironment.h"
#include "cgicc/CgiInput.h"
#include "cgicc/FormEntry.h"
#include "cgicc/FormFile.h"

namespace cgicc
{
std::string form_urldecode(const std::string& s);

class Cgicc
{
  public:
	typedef std::vector<FormEntry>::iterator       form_iterator;
	typedef std::vector<FormEntry>::const_iterator const_form_iterator;
	typedef std::vector<FormFile>::iterator        file_iterator;
	typedef std::vector<FormFile>::const_iterator  const_file_iterator;

	// Real cgicc reads the process environment when input is null; lite always needs a CgiInput.
	explicit Cgicc(CgiInput* input = nullptr);
	Cgicc(const CgiEnvironment& env, const std::string& queryString, const std::string& postData);

	const CgiEnvironment& getEnvironment() const { return env_; }

	form_iterator          getElement(const std::string& name);
	const_form_iterator    getElement(const std::string& name) const;
	std::vector<FormEntry> getElements() const { return entries_; }
	bool                   getElement(const std::string& name, std::vector<FormEntry>& result) const;
	bool                   queryCheckbox(const std::string& name) const { return getElement(name) != entries_.end(); }
	// GNU cgicc shorthand: value of the named form element, or "" if absent
	std::string operator()(const std::string& name) const
	{
		auto it = getElement(name);
		return it == entries_.end() ? std::string() : it->getValue();
	}

	file_iterator                getFile(const std::string& name);
	const_file_iterator          getFile(const std::string& name) const;
	const std::vector<FormFile>& getFiles() const { return files_; }

	// iteration helpers some code uses
	const_form_iterator begin() const { return entries_.begin(); }
	const_form_iterator end() const { return entries_.end(); }

  private:
	void parse(const std::string& data);

	CgiEnvironment         env_;
	std::vector<FormEntry> entries_;
	std::vector<FormFile>  files_;
};
}  // namespace cgicc

#endif
