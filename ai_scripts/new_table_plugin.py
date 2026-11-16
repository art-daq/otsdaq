#!/usr/bin/env python3
"""new_table_plugin.py - scaffold a table plugin: the C++ class that ots creates for a table when it loads a
configuration group, with one typed member per data column of the table and an init() that fills them.

Usage: new_table_plugin.py <TableName> [--info <file>] [--include-dir <dir>] [--into <dir>]
  TableName     the table, ending in Table, letters and digits only; its definition must already exist
                (create it with tableinfo.py: the plugin's constructor reads it, and so does this script)
  --info        the definition file (default $USER_DATA/TableInfo/<TableName>Info.xml)
  --include-dir the include path prefix of the plugin's repo directory, as used in #include (default
                otsdaq/TablePlugins; for another repo e.g. otsdaq-components/UserTableDataFormats)
  --into        where to write the files (default ./<TableName>_scaffold; nothing is written into a repository)

Writes <TableName>.h and <TableName>_table.cc, then prints the next steps (copy, CMake line, build, check).
Most tables need NO plugin: without one ots uses a generic table object built from the definition file, and
everything that reads the table through a ConfigurationTree (front ends, data processors, supervisors) works.
Write a plugin only to run code when the table's group is activated (validation, generating files) or to give
the application the table as a C++ object (configManager->getTable<TableName>("TableName")).
Columns: UID -> recordUID_ (the row's UID); Data/UniqueData/MultilineData/FixedChoiceData/BitMap text ->
std::string; NUMBER -> unsigned int (int if the default is negative, double if it has a '.'); OnOff, TrueFalse,
YesNo -> bool; a column named Status (OnOff) only skips the rows that are Off. Not copied (a comment lists them):
the link and group-ID columns (follow a link with row.getNode("<LinkColumn>")), CommentDescription, Author,
RecordInsertionTime.
The generated code compiles against the otsdaq headers (checked with a syntax-only compile) but has not been
linked or run: the build is the user's (mb). See ai_skills/table-plugins.md.
"""
import os
import re
import sys
import xml.etree.ElementTree as ET

H = '''#ifndef _ots_@NAME@_h_
#define _ots_@NAME@_h_

#include "otsdaq/TableCore/TableBase.h"

#include <string>
#include <vector>

namespace ots
{
/// @NAME@
///	Table plugin for the table @NAME@ (TableInfo/@NAME@Info.xml).
///	Created by ots when a group that contains the table is loaded; init() runs at each activation of such a group.
///	The member names of Row match the column names of the table definition: if the definition changes,
///	change Row and init() with it.
class @NAME@ : public TableBase
{
  public:
	@NAME@(void);
	virtual ~@NAME@(void);

	void init(ConfigurationManager* configManager) override;

	/// One active row of the table: the values init() read.
	struct Row
	{
@ROWMEMBERS@	};

	const std::vector<Row>& getRows(void) const { return rows_; }
	const Row&              getRow(const std::string& recordUID) const;  ///< throws if there is no such active row

  private:
	std::vector<Row> rows_;  ///< filled by init()
};
}  // namespace ots
#endif
'''

CC = '''#include "@INCDIR@/@NAME@.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/Macros/TablePluginMacros.h"

using namespace ots;

//==============================================================================
@NAME@::@NAME@(void) : TableBase("@NAME@")
{
	////////////////////////////////////////////////////////////////////////////
	// WARNING: the field names used in C++ MUST match the Table Info file,
	//	TableInfo/@NAME@Info.xml. The constructor runs when ots loads the plugin,
	//	not when the table is read: do not read table values here.
	////////////////////////////////////////////////////////////////////////////
}

//==============================================================================
@NAME@::~@NAME@(void) {}

//==============================================================================
/// init
///	Called for every table of a group when the group is activated, in every process that loads
///	the group. For a Configuration group this may happen in several threads at once, one per table:
///	do not depend on another table's init() having run (reading other tables through
///	configManager is fine), and protect anything shared between tables.
///	A thrown std::runtime_error (use __SS_THROW__) fails the activation with
///	"Error detected calling @NAME@.init()" and the message.
void @NAME@::init(ConfigurationManager* configManager)
{
	// init() runs again at each activation: start from nothing
	rows_.clear();

	// If init() writes files or starts something that must happen once per context process, add:
	//	if(!configManager->isOwnerFirstAppInContext()) return;

	std::vector<std::pair<std::string, ConfigurationTree>> children =
	    configManager->__SELF_NODE__.getChildren();
	for(auto& childPair : children)
	{
@INITBODY@
		// validate the row: throw to refuse the group, e.g.
		//	if(row.someValue_ == 0)
		//	{
		//		__SS__ << "Row '" << row.recordUID_ << "': someValue must not be 0." << __E__;
		//		__SS_THROW__;
		//	}

		rows_.push_back(row);
	}

	__COUT__ << "@NAME@ initialized with " << rows_.size() << " active row(s)." << __E__;
}  // end init()

//==============================================================================
const @NAME@::Row& @NAME@::getRow(const std::string& recordUID) const
{
	for(const auto& row : rows_)
		if(row.recordUID_ == recordUID)
			return row;

	__SS__ << "@NAME@ has no active row '" << recordUID << "'." << __E__;
	__SS_THROW__;
}  // end getRow()

DEFINE_OTS_TABLE(@NAME@)
'''

TEXT_TYPES = ("Data", "UniqueData", "MultilineData", "FixedChoiceData", "BitMap")
BOOL_TYPES = ("OnOff", "TrueFalse", "YesNo")
SKIP_TYPES = ("Comment", "Author", "Timestamp")


def member_name(col):
    return col[0].lower() + col[1:] + "_"


def number_type(default):
    if "." in default:
        return "double"
    if default.strip().startswith("-"):
        return "int"
    return "unsigned int"


def read_columns(path):
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as e:
        sys.exit("Cannot read the table definition %s: %s" % (path, e))
    cols = [c.attrib for c in root.iter("COLUMN")]
    if not cols:
        sys.exit("%s has no COLUMN entries." % path)
    return cols


def main():
    a = sys.argv[1:]
    inc, into, info = "otsdaq/TablePlugins", None, None
    for opt in ("--include-dir", "--into", "--info"):
        if opt in a:
            i = a.index(opt)
            if i + 1 >= len(a):
                print(__doc__)
                return 1
            val = a[i + 1]
            a = a[:i] + a[i + 2:]
            if opt == "--include-dir":
                inc = val.rstrip("/")
            elif opt == "--into":
                into = val
            else:
                info = val
    if len(a) != 1:
        print(__doc__)
        return 1
    name = a[0]
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9]*Table", name) or name == "Table":
        sys.exit("Name '%s' must be letters and digits and end in Table." % name)
    if info is None:
        if not os.environ.get("USER_DATA"):
            sys.exit("USER_DATA is not set (source setup_ots.sh) and no --info given.")
        info = os.path.join(os.environ["USER_DATA"], "TableInfo", name + "Info.xml")
    if not os.path.exists(info):
        sys.exit("No table definition %s. Create it first, e.g.:\n"
                 "  ai_scripts/tableinfo.py new %s \"Setting:Data:NUMBER:10\" \"Label:Data\"" % (info, name))
    cols = read_columns(info)

    members, body, notcopied = [], [], []
    has_status = any(c.get("Name") == "Status" and c.get("Type") == "OnOff" for c in cols)
    for c in cols:
        cname, ctype, dtype = c.get("Name", ""), c.get("Type", ""), c.get("DataType", "STRING")
        default = c.get("DefaultValue", "")
        if ctype == "UID":
            members.append('\t\tstd::string recordUID_;  ///< UID column %s\n' % cname)
        elif cname == "Status" and ctype == "OnOff" and has_status:
            continue  # used to skip rows
        elif ctype in SKIP_TYPES or ctype.startswith(("ChildLink", "GroupID")):
            notcopied.append("%s (%s)" % (cname, ctype))
        elif ctype in BOOL_TYPES:
            members.append("\t\tbool %s = false;  ///< %s %s\n" % (member_name(cname), cname, ctype))
            body.append('\t\tchildPair.second.getNode("%s").getValue(row.%s);' % (cname, member_name(cname)))
        elif ctype in TEXT_TYPES and dtype == "NUMBER":
            t = number_type(default)
            members.append("\t\t%s %s = 0;  ///< %s (NUMBER)\n" % (t, member_name(cname), cname))
            body.append('\t\tchildPair.second.getNode("%s").getValue(row.%s);' % (cname, member_name(cname)))
        elif ctype in TEXT_TYPES:
            members.append("\t\tstd::string %s;  ///< %s %s\n" % (member_name(cname), cname, ctype))
            body.append('\t\tchildPair.second.getNode("%s").getValue(row.%s);' % (cname, member_name(cname)))
        else:
            notcopied.append("%s (%s, unknown here)" % (cname, ctype))
    if notcopied:
        members.append("\n\t\t// not copied (follow links with childPair.second.getNode(\"<link column>\")): %s\n" % ", ".join(notcopied))

    init = ""
    if has_status:
        init += ('\t\t// only rows with Status On\n\t\tbool status = false;\n'
                 '\t\tchildPair.second.getNode("Status").getValue(status);\n\t\tif(!status)\n\t\t\tcontinue;\n\n')
    init += "\t\tRow row;\n\t\trow.recordUID_ = childPair.first;\n" if any(c.get("Type") == "UID" for c in cols) else "\t\tRow row;\n"
    init += "\n".join(body) + "\n"

    out = into or ("./%s_scaffold" % name)
    os.makedirs(out, exist_ok=True)
    files = (("%s.h" % name, H.replace("@ROWMEMBERS@", "".join(members))),
             ("%s_table.cc" % name, CC.replace("@INITBODY@", init)))
    for fn, text in files:
        path = os.path.join(out, fn)
        if os.path.exists(path):
            sys.exit("%s exists: not overwriting." % path)
        open(path, "w").write(text.replace("@NAME@", name).replace("@INCDIR@", inc))
    print("Wrote %s/%s.h and %s/%s_table.cc from %s" % (out, name, out, name, info))
    print("""
Next steps (the build is the user's: mz_uc for a new file, then mb):
 1. Copy both files into the repository's table plugin directory (include path %s) and add to its CMakeLists.txt:
        cet_build_plugin(%s otsdaq::table)       # after: include(otsdaq::table)
    If other code must link against the class (getTable<%s>), put the logic in %sImpl.cc, build it with
    cet_make_library(LIBRARY_NAME %s SOURCE %sImpl.cc ...) and use LIBRARIES REG <that library> on the plugin
    (DesktopIconTable and IterateTable in otsdaq/TablePlugins/CMakeLists.txt are the examples).
 2. After the build, restart ots. Check the library exists: ls $OTSDAQ_LIB | grep %s_table
    (a missing or unloadable plugin is NOT an error: ots silently uses the generic table and init() never runs).
 3. Check that init() ran: its __COUT__ line in the Gateway log at Configure, or the effect you made it have.
    ai_skills/table-plugins.md.""" % (inc, name, name, name, name, name, name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
