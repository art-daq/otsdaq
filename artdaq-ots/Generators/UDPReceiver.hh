#ifndef artdaq_ots_Generators_UDPReceiver_hh
#define artdaq_ots_Generators_UDPReceiver_hh

// The UDP Receiver class receives UDP data from an otsdaq application and
// puts that data into UDPFragments for further ARTDAQ analysis.
//
// It currently assumes two things to be true:
// 1. The first word of the UDP packet is an 8-bit flag with information
// about the status of the sender
// 2. The second word is an 8-bit sequence ID, used for detecting
// dropped UDP datagrams

// Some C++ conventions used:

// -Append a "_" to every private member function and variable

#include "artdaq-core/Data/Fragment.hh"
#include "artdaq/Generators/CommandableFragmentGenerator.hh"
#include "fhiclcpp/fwd.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <list>
#include <mutex>
#include <queue>
#include <thread>

namespace ots
{
enum class CommandType : uint8_t
{
	Read        = 0,
	Write       = 1,
	Start_Burst = 2,
	Stop_Burst  = 3,
};

enum class ReturnCode : uint8_t
{
	Read   = 0,
	First  = 1,
	Middle = 2,
	Last   = 3,
};

enum class DataType : uint8_t
{
	Raw    = 0,
	JSON   = 1,
	String = 2,
};

struct CommandPacket
{
	CommandType type;
	uint8_t     dataSize;
	uint64_t    address;
	uint64_t    data[182];
};

typedef std::string               packetBuffer_t;
typedef std::list<packetBuffer_t> packetBuffer_list_t;

class UDPReceiver : public artdaq::CommandableFragmentGenerator
{
  public:
	explicit UDPReceiver(fhicl::ParameterSet const& ps);
	virtual ~UDPReceiver();

  protected:
	// The "getNext_" function is used to implement user-specific
	// functionality; it's a mandatory override of the pure virtual
	// getNext_ function declared in CommandableFragmentGenerator

	bool         getNext_(artdaq::FragmentPtrs& output) override;
	void         start(void) override;
	virtual void start_();
	virtual void stop(void) override;
	virtual void stopNoMutex(void) override;

	virtual void ProcessData_(artdaq::FragmentPtrs& output, size_t totalSize);

	DataType   getDataType(uint8_t byte) { return static_cast<DataType>((byte & 0xF0) >> 4); }
	ReturnCode getReturnCode(uint8_t byte) { return static_cast<ReturnCode>(byte & 0xF); }
	void       send(CommandType flag);

	packetBuffer_list_t packetBuffers_;

	bool        rawOutput_;
	std::string rawPath_;

	// FHiCL-configurable variables. Note that the C++ variable names
	// are the FHiCL variable names with a "_" appended

	int         dataport_;
	std::string ip_;
	int         rcvbuf_;

	// The packet number of the next packet. Used to discover dropped packets
	uint8_t expectedPacketNumber_;

	// Socket parameters
	struct sockaddr_in si_data_;
	int                datasocket_;
	bool               sendCommands_;

  private:
	void receiveLoop_();
	bool isTimerExpired_();

	std::unique_ptr<std::thread> receiverThread_;
	std::mutex                   receiveBufferLock_;
	packetBuffer_list_t          receiveBuffers_;

	bool fakeDataMode_;

	// Number of milliseconds per fragment
	double                                         fragmentWindow_;
	std::chrono::high_resolution_clock::time_point lastFrag_;
};
}  // namespace ots

#endif /* artdaq_demo_Generators_ToySimulator_hh */
