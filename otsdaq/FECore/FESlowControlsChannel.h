#ifndef _ots_FESlowControlsChannel_h_
#define _ots_FESlowControlsChannel_h_

#include <iostream>
#include <string>

namespace ots
{
class FEVInterface;

class FESlowControlsChannel
{
	// clang-format off
  public:
	FESlowControlsChannel(FEVInterface* interface,
	                      const std::string& channelName,
	                      const std::string& dataType,
	                      const std::string& universalAddress,
	                      unsigned int       universalDataBitOffset,
	                      bool               readAccess,
	                      bool               writeAccess,
	                      bool               monitoringEnabled,
	                      bool               recordChangesOnly,
	                      time_t             delayBetweenSamples,
	                      bool               saveEnabled,
	                      const std::string& savePath,
	                      const std::string& saveFileRadix,
	                      bool               saveBinaryFormat,
	                      bool               alarmsEnabled,
	                      bool               latchAlarms,
	                      const std::string& lolo,
	                      const std::string& lo,
	                      const std::string& hi,
	                      const std::string& hihi);

	~FESlowControlsChannel();


	void					print						(std::ostream& out = std::cout) const;

	unsigned int			getReadSizeBytes 			() const { return sizeOfReadBytes_; }
	time_t					getLastSampleTime 			() const { return lastSampleTime_; }
	void					getSample					(std::string& readValue);	
	void  					handleSample				(const std::string& universalReadValue, std::string& txBuffer, FILE* fpAggregate = 0, bool aggregateIsBinaryFormat = false);
	void  					clearAlarms					(int targetAlarm = -1);  // default to all

	static std::string 		underscoreString			(const std::string& str);

  private:
	void 					extractSample				();
	char 					checkAlarms					(std::string& txBuffer);
	void 					convertStringToBuffer		(const std::string& inString, std::string& buffer, bool useDataType = false);

	FEVInterface* 			interface_;

  public:
	const std::string 		channelName_;
	const std::string 		fullChannelName_;
	const std::string 		dataType_;

  private:
	unsigned int 			sizeOfDataTypeBits_;  // defines the size of all data string buffers,
	                                   // must be less than or equal to universalDataSize
	unsigned int  			sizeOfDataTypeBytes_, sizeOfReadBytes_;
	unsigned int  			universalDataBitOffset_;
	unsigned char 			txPacketSequenceNumber_;

  public:
	const bool   			readAccess_, writeAccess_, monitoringEnabled_;
	const bool   			recordChangesOnly_;
	const time_t 			delayBetweenSamples_;

	const bool        		saveEnabled_;
	const std::string 		savePath_;
	const std::string 		saveFileRadix_;
	const bool        		saveBinaryFormat_;

	const bool 				alarmsEnabled_, latchAlarms_;
	std::string 			universalReadValue_;
	std::string  			universalAddress_;    // get size from parent FE interface

  private:
	std::string 			sample_, lastSample_;
	std::string 			lolo_, lo_, hi_, hihi_;
	time_t      			lastSampleTime_;
	bool        			loloAlarmed_, loAlarmed_, hiAlarmed_, hihiAlarmed_;

	std::string 			saveFullFileName_;

	// clang-format on
};

}  // namespace ots

#endif
