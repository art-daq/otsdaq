#ifndef ots_TimeFormatter_h
#define ots_TimeFormatter_h

#include <sys/time.h>
#include <string>

namespace ots
{
class TimeFormatter
{
  public:
	TimeFormatter(std::string source);
	~TimeFormatter(void);

	// Static memebers
	static std::string getTime(void);
	static std::string getmSecTime(void);

	void           stopTimer(void);
	struct tm*     getITime(void);
	struct timeval getImSecTime(void);

  private:
	struct timeval startTime_;
	struct timeval endTime_;
	std::string    origin_;
	bool           verbose_;
};

}  // namespace ots
#endif
