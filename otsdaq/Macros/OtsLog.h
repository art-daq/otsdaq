#ifndef _ots_OtsLog_h_
#define _ots_OtsLog_h_

/// OtsLog.h
///	ots maxlite (OTS_MAXLITE) replacement for the TRACE macros otsdaq uses:
///	TLOG, TLOG_DEBUG/INFO/WARNING/ERROR/TRACE, TTEST and the TLVL_* levels.
///	Included by CoutMacros.h in place of TRACE/tracemf.h, so all __COUT__ style macros
///	work unchanged.
///
///	Messages at or below the threshold level are written to standard output, one line
///	per message in a single write (lines from different threads do not interleave).
///	A message above the threshold costs one integer compare: its stream arguments are
///	not evaluated.
///	The threshold is the environment variable OTS_COUT_LEVEL (a number, default 8 =
///	TLVL_DEBUG, the same levels ots lite prints with TRACE_LVLS=0x1ff); it can also be
///	changed at run time with ots::log::setThreshold().

#include <sstream>
#include <string>
#include <string_view>

/// same level numbers as TRACE, so level arithmetic like TLVL_DEBUG + 20 is unchanged
enum ots_tlvl_t : int
{
	TLVL_FATAL   = 0,
	TLVL_EMERG   = TLVL_FATAL,
	TLVL_ALERT   = 1,
	TLVL_CRIT    = 2,
	TLVL_ERROR   = 3,
	TLVL_WARNING = 4,
	TLVL_WARN    = TLVL_WARNING,
	TLVL_NOTICE  = 5,
	TLVL_INFO    = 6,
	TLVL_LOG     = 7,
	TLVL_DEBUG   = 8,
	TLVL_DBG     = TLVL_DEBUG,
	TLVL_TRACE   = 9,
	TLVL_DEBUG_1 = 9,
};

namespace ots
{
namespace log
{
/// current threshold: messages with level <= threshold are printed
int         threshold(void);
void        setThreshold(int level);
inline bool enabled(int level) { return level <= threshold(); }

/// writes one finished message (level name, subject, file:line, text) to standard output
void emit(int                level,
          const std::string& subject,
          const char*        file,
          int                line,
          const std::string& text);

/// Line collects one message; used only through the TLOG macro below.
class Line
{
  public:
	Line(int level, std::string_view subject, const char* file, int line)
	    : level_(level), pending_(enabled(level)), file_(file), line_(line)
	{
		if(pending_)
			subject_.assign(subject.data(), subject.size());
	}
	bool                pending(void) const { return pending_; }
	std::ostringstream& os(void) { return os_; }
	void                finish(void)
	{
		pending_ = false;
		emit(level_, subject_, file_, line_, os_.str());
	}

  private:
	int                level_;
	bool               pending_;
	const char*        file_;
	int                line_;
	std::string        subject_;
	std::ostringstream os_;
};
}  // namespace log
}  // namespace ots

// clang-format off
/// TLOG(level) or TLOG(level, subject); used as TLOG(...) << a << b;
/// The for-statement form makes this a single statement (safe in if/else without braces)
/// and skips evaluating the << arguments when the level is not enabled.
#define OTS_TLOG_PICK_(_1, _2, NAME, ...) NAME
#define OTS_TLOG2_(L, N) for(::ots::log::Line otsLogLine_((L), (N), __FILE__, __LINE__); otsLogLine_.pending(); otsLogLine_.finish()) otsLogLine_.os()
#define OTS_TLOG1_(L) OTS_TLOG2_(L, (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__))
#define TLOG(...) OTS_TLOG_PICK_(__VA_ARGS__, OTS_TLOG2_, OTS_TLOG1_, unused)(__VA_ARGS__)

#define TLOG_ERROR(...)		TLOG(TLVL_ERROR __VA_OPT__(,) __VA_ARGS__)
#define TLOG_WARNING(...)	TLOG(TLVL_WARNING __VA_OPT__(,) __VA_ARGS__)
#define TLOG_INFO(...)		TLOG(TLVL_INFO __VA_OPT__(,) __VA_ARGS__)
#define TLOG_TRACE(...)		TLOG(TLVL_TRACE __VA_OPT__(,) __VA_ARGS__)
#define TLOG_DEBUG(LVL, ...)	TLOG(TLVL_DEBUG + (LVL) __VA_OPT__(,) __VA_ARGS__)

/// true if messages at this level are printed
#define TTEST(LVL)			(::ots::log::enabled(LVL))
// clang-format on

#endif
