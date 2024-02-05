#ifndef _ots_TCPPacket_h_
#define _ots_TCPPacket_h_

#include <cstdint>
#include <string>

namespace ots
{
class TCPPacket
{
  public:
	TCPPacket();
	virtual ~TCPPacket(void);

	static std::string encode(char const* message, std::size_t length);
	static std::string encode(const std::string& message);

	bool decode(std::string& message);
	// Resets the storage buffer
	void reset(void);
	bool isEmpty(void);

	// Operator overload
	TCPPacket& operator+=(const std::string& buffer)
	{
		this->fBuffer += buffer;
		return *this;
	}

	friend std::ostream& operator<<(std::ostream& out, const TCPPacket& packet)
	{
		// out << packet.fBuffer.substr(TCPPacket::headerLength);
		out << packet.fBuffer;

		return out;  // return std::ostream so we can chain calls to operator<<
	}

  private:
	static constexpr uint32_t headerLength = 4;  // sizeof(uint32_t); //THIS MUST BE 4

	std::string fBuffer;  // This is Header + Message
};
}  // namespace ots
#endif
