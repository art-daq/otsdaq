#ifndef _ots_RunControlIterationConstants_h_
#define _ots_RunControlIterationConstants_h_

namespace ots
{
struct RunControlIterationConstants
{
	// Iteration index after which startup actions can trigger event generation.
	static constexpr int RUN_START_READY_FOR_TRIGGERS_ITERATION = 12;
};
}  // namespace ots

#endif
