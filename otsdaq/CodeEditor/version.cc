#include "otsdaq/CodeEditor/version.h"
#include <config/version.h>
#include <xcept/version.h>
#include <xdaq/version.h>

GETPACKAGEINFO(CodeEditor)

void CodeEditor::checkPackageDependencies()
{
	CHECKDEPENDENCY(config);
	CHECKDEPENDENCY(xcept);
	CHECKDEPENDENCY(xdaq);
}

std::set<std::string, std::less<std::string> > CodeEditor::getPackageDependencies()
{
	std::set<std::string, std::less<std::string> > dependencies;

	ADDDEPENDENCY(dependencies, config);
	ADDDEPENDENCY(dependencies, xcept);
	ADDDEPENDENCY(dependencies, xdaq);

	return dependencies;
}
