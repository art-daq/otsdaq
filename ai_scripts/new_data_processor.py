#!/usr/bin/env python3
"""new_data_processor.py - scaffold a data processor plugin (a Producer that fills a Data Buffer, or a Consumer that
drains it) with the same structure as the plugins of otsdaq (UDPDataListenerProducer, RawDataSaverConsumer).

Usage: new_data_processor.py producer|consumer <Name> [--include-dir <dir>] [--into <dir>]
  Name          class name; must end in Producer (producer) or Consumer (consumer), letters and digits only
  --include-dir the include path prefix of the plugin's repo directory, as used in #include (default
                otsdaq/DataProcessorPlugins; for another repo e.g. otsdaq-components/DataProcessorPlugins)
  --into        where to write the files (default ./<Name>_scaffold; nothing is written into a repository)

Writes <Name>.h and <Name>_processor.cc, then prints the next steps: copy them into the repository's plugin directory,
the CMake line, the settings table command (tableinfo.py) and the DataProcessorTable row (cfg_udp_edit_group.sh).
The generated code compiles against the installed otsdaq headers (checked with a syntax-only compile) but has not been
linked or run: the build is the user's (mb). See ai_skills/data-processor-plugins.md.

The producer writes synthetic packets (a counter) every PacketIntervalMs: usable without hardware to exercise the
buffer, the saver and the Visualizer. The consumer counts packets and bytes and logs every LogEveryNPackets.
"""
import os
import re
import sys

PRODUCER_H = '''#ifndef _ots_@NAME@_h_
#define _ots_@NAME@_h_

#include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/DataManager/DataProducer.h"

#include <cstdint>
#include <map>
#include <string>

namespace ots
{
class ConfigurationTree;

/// @NAME@
///	Producer plugin: writes a synthetic packet (2-byte header: type, sequence; then an 8-byte counter, then padding)
///	into its Data Buffer every PacketIntervalMs. Replace workLoopThread()/write*() with the real data source.
class @NAME@ : public DataProducer, public Configurable
{
  public:
	@NAME@(std::string              supervisorApplicationUID,
	       std::string              bufferUID,
	       std::string              processorUID,
	       const ConfigurationTree& theXDAQContextConfigTree,
	       const std::string&       configurationPath);
	virtual ~@NAME@(void);

	void startProcessingData(std::string runNumber) override;

  protected:
	bool workLoopThread(toolbox::task::WorkLoop* workLoop) override;
	void fastWrite(void);

	unsigned int  packetBytes_;       ///< size of each packet, from the settings table
	unsigned int  packetIntervalMs_;  ///< pause between packets
	uint64_t      counter_;
	unsigned char sequence_;

	/// For fast write: the sub-buffer attached in the buffer
	std::string*                        dataP_;
	std::map<std::string, std::string>* headerP_;
};
}  // namespace ots

#endif
'''

PRODUCER_CC = '''#include "@INCDIR@/@NAME@.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <string.h>
#include <unistd.h>

using namespace ots;

//==============================================================================
@NAME@::@NAME@(std::string              supervisorApplicationUID,
               std::string              bufferUID,
               std::string              processorUID,
               const ConfigurationTree& theXDAQContextConfigTree,
               const std::string&       configurationPath)
    : WorkLoop(processorUID)
    , DataProducer(supervisorApplicationUID,
                   bufferUID,
                   processorUID,
                   theXDAQContextConfigTree.getNode(configurationPath)
                       .getNode("BufferSize")
                       .getValue<unsigned int>())  // number of sub-buffers of this producer
    , Configurable(theXDAQContextConfigTree, configurationPath)
    , packetBytes_(theXDAQContextConfigTree.getNode(configurationPath)
                       .getNode("PacketBytes")
                       .getValue<unsigned int>())
    , packetIntervalMs_(theXDAQContextConfigTree.getNode(configurationPath)
                            .getNode("PacketIntervalMs")
                            .getValue<unsigned int>())
    , counter_(0)
    , sequence_(0)
    , dataP_(nullptr)
    , headerP_(nullptr)
{
	if(packetBytes_ < 10)
		packetBytes_ = 10;  // header + counter
	__CFG_COUT__ << "Configured: packetBytes=" << packetBytes_
	             << " packetIntervalMs=" << packetIntervalMs_ << __E__;
}  // end constructor

//==============================================================================
@NAME@::~@NAME@(void) {}

//==============================================================================
/// a run starts with an empty count (a pause/resume calls this with an empty run number)
void @NAME@::startProcessingData(std::string runNumber)
{
	if(runNumber != "")
		counter_ = 0;
	DataProducer::startProcessingData(runNumber);  // starts the work loop thread
}  // end startProcessingData()

//==============================================================================
/// called repeatedly by the work loop thread while the run is going; return false to end the loop
bool @NAME@::workLoopThread(toolbox::task::WorkLoop* /*workLoop*/)
{
	fastWrite();
	usleep(packetIntervalMs_ * 1000);
	return WorkLoop::continueWorkLoop_;
}  // end workLoopThread()

//==============================================================================
void @NAME@::fastWrite(void)
{
	// attach to the next free sub-buffer; none free means a consumer is behind: wait and retry
	if(DataProducer::attachToEmptySubBuffer(dataP_, headerP_) < 0)
	{
		__CFG_COUT__ << "There are no available buffers! Retrying after 10 ms..." << __E__;
		countError();
		usleep(10000);
		return;
	}

	std::string& packet = *dataP_;
	packet.assign(packetBytes_, '\\0');
	packet[0] = 1;                                    // type: 1 = first of a burst, 2 = middle, 3 = last
	packet[1] = static_cast<char>(sequence_++);       // 1-byte sequence, wraps at 256
	memcpy(&packet[2], &counter_, sizeof(counter_));  // payload: 8-byte counter
	++counter_;

	(*headerP_)["IPAddress"] = "0.0.0.0";  // header: free-form string map that travels with the packet
	(*headerP_)["Port"]      = "0";

	countPacket(packet.size());  // throughput statistics (DataProcessor)
	DataProducer::setWrittenSubBuffer<std::string, std::map<std::string, std::string>>();
}  // end fastWrite()

DEFINE_OTS_PROCESSOR(@NAME@)
'''

CONSUMER_H = '''#ifndef _ots_@NAME@_h_
#define _ots_@NAME@_h_

#include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/DataManager/DataConsumer.h"

#include <cstdint>
#include <map>
#include <string>

namespace ots
{
class ConfigurationTree;

/// @NAME@
///	Consumer plugin: reads every packet from its Data Buffer, counts packets and bytes, and logs every
///	LogEveryNPackets. Replace process() with the real work (save, decode, forward, histogram).
class @NAME@ : public DataConsumer, public Configurable
{
  public:
	@NAME@(std::string              supervisorApplicationUID,
	       std::string              bufferUID,
	       std::string              processorUID,
	       const ConfigurationTree& theXDAQContextConfigTree,
	       const std::string&       configurationPath);
	virtual ~@NAME@(void);

	void startProcessingData(std::string runNumber) override;
	void stopProcessingData(void) override;

  protected:
	bool workLoopThread(toolbox::task::WorkLoop* workLoop) override;
	void fastRead(void);
	void process(const std::string& data, const std::map<std::string, std::string>& header);

	uint64_t logEveryNPackets_;
	uint64_t packets_;
	uint64_t bytes_;

	/// For fast read: points into the buffer, no copy
	std::string*                        dataP_;
	std::map<std::string, std::string>* headerP_;
};
}  // namespace ots

#endif
'''

CONSUMER_CC = '''#include "@INCDIR@/@NAME@.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <unistd.h>

using namespace ots;

//==============================================================================
@NAME@::@NAME@(std::string              supervisorApplicationUID,
               std::string              bufferUID,
               std::string              processorUID,
               const ConfigurationTree& theXDAQContextConfigTree,
               const std::string&       configurationPath)
    : WorkLoop(processorUID)
    , DataConsumer(supervisorApplicationUID,
                   bufferUID,
                   processorUID,
                   HighConsumerPriority)  // note: the buffer registers every consumer as high priority
    , Configurable(theXDAQContextConfigTree, configurationPath)
    , logEveryNPackets_(theXDAQContextConfigTree.getNode(configurationPath)
                            .getNode("LogEveryNPackets")
                            .getValue<uint64_t>())
    , packets_(0)
    , bytes_(0)
    , dataP_(nullptr)
    , headerP_(nullptr)
{
}  // end constructor

//==============================================================================
@NAME@::~@NAME@(void) {}

//==============================================================================
void @NAME@::startProcessingData(std::string runNumber)
{
	if(runNumber != "")  // an empty run number is a resume after pause: keep counting
	{
		packets_ = 0;
		bytes_   = 0;
		// open files, reset state for the new run here
	}
	DataConsumer::startProcessingData(runNumber);  // starts the work loop thread
}  // end startProcessingData()

//==============================================================================
void @NAME@::stopProcessingData(void)
{
	DataConsumer::stopProcessingData();  // ends the work loop thread
	__CFG_COUT__ << "Stopped after " << packets_ << " packets, " << bytes_ << " bytes." << __E__;
	// close files here
}  // end stopProcessingData()

//==============================================================================
bool @NAME@::workLoopThread(toolbox::task::WorkLoop* /*workLoop*/)
{
	fastRead();
	return WorkLoop::continueWorkLoop_;
}  // end workLoopThread()

//==============================================================================
void @NAME@::fastRead(void)
{
	// point at the next written sub-buffer; a negative result means nothing to read yet
	if(DataConsumer::read(dataP_, headerP_) < 0)
	{
		usleep(100);
		return;
	}

	process(*dataP_, *headerP_);

	// hand the sub-buffer back: the producer can reuse it once EVERY consumer of the buffer has done this
	DataConsumer::setReadSubBuffer<std::string, std::map<std::string, std::string>>();
}  // end fastRead()

//==============================================================================
void @NAME@::process(const std::string& data, const std::map<std::string, std::string>& /*header*/)
{
	++packets_;
	bytes_ += data.size();
	countPacket(data.size());  // throughput statistics (DataProcessor)
	if(logEveryNPackets_ && packets_ % logEveryNPackets_ == 0)
		__CFG_COUT__ << "Packets: " << packets_ << " bytes: " << bytes_ << __E__;
}  // end process()

DEFINE_OTS_PROCESSOR(@NAME@)
'''


def main():
    a = sys.argv[1:]
    inc, into = "otsdaq/DataProcessorPlugins", None
    for opt in ("--include-dir", "--into"):
        if opt in a:
            i = a.index(opt)
            val = a[i + 1]
            a = a[:i] + a[i + 2:]
            if opt == "--include-dir":
                inc = val.rstrip("/")
            else:
                into = val
    if len(a) != 2 or a[0] not in ("producer", "consumer"):
        print(__doc__)
        return 1
    kind, name = a
    suffix = "Producer" if kind == "producer" else "Consumer"
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9]*" + suffix, name) or name == suffix:
        sys.exit("Name '%s' must be letters and digits and end in %s." % (name, suffix))
    out = into or ("./%s_scaffold" % name)
    os.makedirs(out, exist_ok=True)
    h, cc = (PRODUCER_H, PRODUCER_CC) if kind == "producer" else (CONSUMER_H, CONSUMER_CC)
    for fn, text in (("%s.h" % name, h), ("%s_processor.cc" % name, cc)):
        path = os.path.join(out, fn)
        if os.path.exists(path):
            sys.exit("%s exists: not overwriting." % path)
        open(path, "w").write(text.replace("@NAME@", name).replace("@INCDIR@", inc))
    print("Wrote %s/%s.h and %s/%s_processor.cc" % (out, name, out, name))
    if kind == "producer":
        cols = '"BufferSize:Data:NUMBER:100" "PacketBytes:Data:NUMBER:64" "PacketIntervalMs:Data:NUMBER:10"'
    else:
        cols = '"LogEveryNPackets:Data:NUMBER:1000"'
    print("""
Next steps (the build is the user's: mb):
 1. Copy both files into the repository's plugin directory (include path %s) and add to its CMakeLists.txt:
        cet_build_plugin(%s otsdaq::dataProcessor)      # after: include(otsdaq::dataProcessor)
 2. The plugin's settings table (a row per instance; column names must match what the constructor reads):
        ai_scripts/tableinfo.py new %sTable %s
    then restart ots so the table definition is loaded (or: the Configuration GUI re-reads it at once).
 3. A DataProcessorTable row in the Configuration group (ProcessorType %s, ProcessorPluginName %s,
    LinkToProcessorTable %sTable, LinkToProcessorUID <your row>, Status On) and a settings row:
    ai_skills/data-processor-plugins.md, "Using a plugin".""" % (inc, name, name, cols, suffix, name, name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
