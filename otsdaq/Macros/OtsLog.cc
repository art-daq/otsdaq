#include "otsdaq/Macros/OtsLog.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <mutex>
#include <regex>
#include <sstream>

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
//==============================================================================
/// Web Console sender: the Console Supervisor receives UDP lines through otsConsoleFwd.
/// With messagefacility (lite/full) its UDP destination sends them; in maxlite this does,
/// to the UDP destination named in the fcl file of OTSDAQ_LOG_FHICL (set by the ots
/// script, e.g. MessageFacilityGen.fcl: otsConsole: { type: UDP port: 30000 host: ... }),
/// in the same format: UDPMFMESSAGE|time|seq|host|hostaddr|severity|category|application|
/// pid|iteration|module|file|line|message
struct ConsoleSender
{
	int                fd = -1;
	struct sockaddr_in destination;
	std::string        hostname, hostAddress;
	std::string        pid;
	uint32_t           sequence = 0;  // guarded by sendMutex
	std::mutex  sendMutex;  // numbering and sending in one step keeps lines in order
	std::mutex  appMutex;
	std::string application;

	ConsoleSender()
	{
		const char* fclPath = getenv("OTSDAQ_LOG_FHICL");
		if(!fclPath || !*fclPath)
			return;
		std::ifstream in(fclPath);
		if(!in)
			return;
		std::stringstream ss;
		ss << in.rdbuf();
		const std::string fcl = ss.str();

		// find the destination block whose type is UDP
		static const std::regex block(R"re((\w+)\s*:\s*\{([^}]*)\})re");
		static const std::regex typeUdp(R"re(type\s*:\s*"?UDP"?)re");
		static const std::regex portRe(R"re(port\s*:\s*([0-9]+))re");
		static const std::regex hostRe(R"re(host\s*:\s*"?([^"\s]+)"?)re");
		std::string             host = "127.0.0.1";
		int                     port = 0;
		for(auto it = std::sregex_iterator(fcl.begin(), fcl.end(), block);
		    it != std::sregex_iterator();
		    ++it)
		{
			const std::string body = (*it)[2];
			if(!std::regex_search(body, typeUdp))
				continue;
			std::smatch m;
			if(std::regex_search(body, m, portRe))
				port = atoi(m[1].str().c_str());
			if(std::regex_search(body, m, hostRe))
				host = m[1];
			break;
		}
		if(port <= 0)
			return;

		struct addrinfo hints, *res = nullptr;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family   = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		if(getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res)
			return;
		memcpy(&destination, res->ai_addr, sizeof(destination));
		freeaddrinfo(res);
		destination.sin_port = htons((uint16_t)port);

		fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
		if(fd >= 0)
		{
			int bufferSize = 4 * 1024 * 1024;  // absorb bursts (e.g. during Configure)
			setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize));
		}

		char hostBuf[256] = "";
		gethostname(hostBuf, sizeof(hostBuf) - 1);
		hostname    = hostBuf;
		hostAddress = "127.0.0.1";
		if(getaddrinfo(hostBuf, nullptr, &hints, &res) == 0 && res)
		{
			char addrBuf[INET_ADDRSTRLEN] = "";
			inet_ntop(AF_INET,
			          &((struct sockaddr_in*)res->ai_addr)->sin_addr,
			          addrBuf,
			          sizeof(addrBuf));
			hostAddress = addrBuf;
			freeaddrinfo(res);
		}
		pid             = std::to_string(getpid());
		const char* app = getenv("OTS_APPLICATION_NAME");
		application     = app ? app : "ots";
	}

	void send(int                level,
	          const std::string& subject,
	          const char*        shortFile,
	          int                line,
	          const std::string& text,
	          size_t             textEnd,
	          const struct tm&   tmv)
	{
		if(fd < 0)
			return;
		const char* severity = level <= TLVL_ERROR     ? "Error"
		                       : level <= TLVL_WARNING ? "Warning"
		                       : level <= TLVL_INFO    ? "Info"
		                                               : "Debug";
		char        stamp[64];
		strftime(stamp, sizeof(stamp), "%d-%b-%Y %H:%M:%S", &tmv);
		std::string app;
		{
			std::lock_guard<std::mutex> lock(appMutex);
			app = application;
		}
		std::string msg;
		msg.reserve(textEnd + subject.size() + 160);
		msg += "UDPMFMESSAGE|";
		msg += stamp;
		msg += '|';
		const size_t sequencePosition =
		    msg.size();  // sequence number inserted under the lock
		msg += '|';
		msg += hostname;
		msg += '|';
		msg += hostAddress;
		msg += '|';
		msg += severity;
		msg += '|';
		msg += subject;
		msg += '|';
		msg += app;
		msg += '|';
		msg += pid;
		msg += "||";  // iteration, module
		msg += '|';
		msg += shortFile;
		msg += '|';
		msg += std::to_string(line);
		msg += '|';
		// a NUL byte would split the packet into two Console messages (the second one
		// without fields): send it as the two characters backslash and 0
		for(size_t i = 0; i < textEnd; ++i)
			if(text[i] == '\0')
				msg += "\\0";
			else
				msg += text[i];

		// the Console checks that each process's sequence numbers arrive in order
		std::lock_guard<std::mutex> lock(sendMutex);
		msg.insert(sequencePosition, std::to_string(++sequence));
		sendto(fd,
		       msg.data(),
		       msg.size(),
		       0,
		       (const struct sockaddr*)&destination,
		       sizeof(destination));  // best effort: a lost line is not an error
	}
};

ConsoleSender& consoleSender(void)
{
	static ConsoleSender sender;  // first use (thread-safe static initialization)
	return sender;
}
}  // namespace

void ots::log::setApplicationName(const std::string& name)
{
	ConsoleSender&              sender = consoleSender();
	std::lock_guard<std::mutex> lock(sender.appMutex);
	sender.application = name;
}

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

	consoleSender().send(
	    level, subject, shortFile, line, text, textEnd, tmv);  // web Console
}  // end emit()
