#include "otsdaq/Macros/OtsLog.h"

#include <sys/time.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace
{
int initialThreshold(void)
{
	const char* env = getenv("OTS_COUT_LEVEL");
	if(env && *env)
	{
		char* end   = nullptr;
		long  level = strtol(env, &end, 0);
		if(end != env)
			return (int)level;
	}
	return TLVL_DEBUG;
}

std::atomic<int> threshold_(initialThreshold());

const char* levelName(int level, char* buf, size_t size)
{
	switch(level)
	{
	case TLVL_FATAL:
		return "FATAL";
	case TLVL_ALERT:
		return "ALERT";
	case TLVL_CRIT:
		return "CRIT";
	case TLVL_ERROR:
		return "ERROR";
	case TLVL_WARNING:
		return "WARNING";
	case TLVL_NOTICE:
		return "NOTICE";
	case TLVL_INFO:
		return "INFO";
	case TLVL_LOG:
		return "LOG";
	case TLVL_DEBUG:
		return "DEBUG";
	default:
		snprintf(buf, size, "DEBUG+%d", level - TLVL_DEBUG);
		return buf;
	}
}
}  // namespace

int  ots::log::threshold(void) { return threshold_.load(std::memory_order_relaxed); }
void ots::log::setThreshold(int level)
{
	threshold_.store(level, std::memory_order_relaxed);
}

//==============================================================================
/// one line: "HH:MM:SS.mmm LEVEL subject file:line | text"
///	(file shown from the repository name on, i.e. after /srcs/)
void ots::log::emit(int                level,
                    const std::string& subject,
                    const char*        file,
                    int                line,
                    const std::string& text)
{
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	struct tm tmv;
	localtime_r(&tv.tv_sec, &tmv);
	char stamp[64];
	snprintf(stamp,
	         sizeof(stamp),
	         "%02d:%02d:%02d.%03d",
	         tmv.tm_hour,
	         tmv.tm_min,
	         tmv.tm_sec,
	         (int)(tv.tv_usec / 1000));

	const char* srcs      = strstr(file, "/srcs/");
	const char* shortFile = srcs ? srcs + 6 : file;

	size_t textEnd =
	    text.size();  // drop trailing new lines (messages often end in std::endl)
	while(textEnd && (text[textEnd - 1] == '\n' || text[textEnd - 1] == '\r'))
		--textEnd;

	char        levelBuf[24];
	std::string out;
	out.reserve(textEnd + subject.size() + 96);
	out += stamp;
	out += ' ';
	out += levelName(level, levelBuf, sizeof(levelBuf));
	out += ' ';
	out += subject;
	out += ' ';
	out += shortFile;
	out += ':';
	out += std::to_string(line);
	out += " |\t";
	out.append(text, 0, textEnd);
	out += '\n';

	fwrite(out.data(), 1, out.size(), stdout);  // one write per line
	fflush(
	    stdout);  // ots sends stdout to a log file: keep it current (and complete on a crash)
}  // end emit()
