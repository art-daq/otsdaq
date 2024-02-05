////////////////////////////////////////////////////////////////////////
// Class:       UDPDump
// Module Type: analyzer
// File:        UDPDump_module.cc
// Description: Prints out information about each event.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "canvas/Utilities/Exception.h"

#include "artdaq-core/Data/Fragment.hh"
#include "artdaq-ots/Overlays/UDPFragment.hh"
#include "otsdaq/Macros/CoutMacros.h"

#include <boost/asio.hpp>
using boost::asio::ip::udp;

#include <arpa/inet.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

namespace ots
{
class JSONDispatcher : public art::EDAnalyzer
{
  public:
	explicit JSONDispatcher(fhicl::ParameterSet const& pset);
	virtual ~JSONDispatcher();

	virtual void analyze(art::Event const& evt) override;
	virtual void beginRun(art::Run const& run) override;

  private:
	int                     prescale_;
	std::string             raw_data_label_;
	std::string             frag_type_;
	boost::asio::io_service io_service_;
	udp::socket             socket_;
	udp::endpoint           remote_endpoint_;
};

}  // namespace ots

ots::JSONDispatcher::JSONDispatcher(fhicl::ParameterSet const& pset)
    : art::EDAnalyzer(pset)
    , prescale_(pset.get<int>("prescale", 1))
    , raw_data_label_(pset.get<std::string>("raw_data_label", "daq"))
    , frag_type_(pset.get<std::string>("frag_type", "UDP"))
    , io_service_()
    , socket_(io_service_, udp::v4())
{
	std::cout << __COUT_HDR_FL__ << "JSONDispatcher Constructor Start" << std::endl;
	int port = pset.get<int>("port", 35555);
	// std::cout << __COUT_HDR_FL__ << "JSONDispatcher port is " << std::to_string(port)
	// << std::endl;  std::cout << __COUT_HDR_FL__ << "JSONDispatcher setting
	// reuse_address option" << std::endl;
	boost::system::error_code ec;
	socket_.set_option(boost::asio::socket_base::reuse_address(true), ec);
	if(ec)
	{
		TLOG(TLVL_ERROR, "JSONDispatcher") << "An error occurred setting reuse_address: " << ec.message() << std::endl;
	}
	// std::cout << __COUT_HDR_FL__ << "JSONDispatcher setting broadcast option" <<
	// std::endl;
	socket_.set_option(boost::asio::socket_base::broadcast(true), ec);
	if(ec)
	{
		TLOG(TLVL_ERROR, "JSONDispatcher") << "An error occurred setting broadcast: " << ec.message() << std::endl;
	}

	// std::cout << __COUT_HDR_FL__ << "JSONDispatcher gettting UDP endpoint" <<
	// std::endl;
	remote_endpoint_ = udp::endpoint(boost::asio::ip::address_v4::broadcast(), port);
	std::cout << __COUT_HDR_FL__ << "JSONDispatcher Constructor End" << std::endl;
}

ots::JSONDispatcher::~JSONDispatcher() {}

void ots::JSONDispatcher::beginRun(art::Run const& run) { std::cout << __COUT_HDR_FL__ << "JSONDispatcher beginning run " << run.run() << std::endl; }

void ots::JSONDispatcher::analyze(art::Event const& evt)
{
	// std::cout << __COUT_HDR_FL__ << "JSONDispatcher getting event number to check
	// prescale" << std::endl;
	art::EventNumber_t eventNumber = evt.event();
	TLOG(TLVL_INFO, "JSONDispatcher") << "Received event with sequence ID " << eventNumber;
	if((int)eventNumber % prescale_ == 0)
	{
		// std::cout << __COUT_HDR_FL__ << "JSONDispatcher dispatching event" <<
		// std::endl;
		std::ostringstream outputJSON;
		outputJSON << "{\"run\":" << std::to_string(evt.run()) << ",\"subrun\":" << std::to_string(evt.subRun())
		           << ",\"event\":" << std::to_string(eventNumber);

		// ***********************
		// *** UDP Fragments ***
		// ***********************

		// look for raw UDP data

		// std::cout << __COUT_HDR_FL__ << "JSONDispatcher getting handle on Fragments" <<
		// std::endl;
		art::Handle<artdaq::Fragments> raw;
		evt.getByLabel(raw_data_label_, frag_type_, raw);
		outputJSON << ",\"fragments\":[";

		if(raw.isValid())
		{
			// std::cout << __COUT_HDR_FL__ << "JSONDispatcher dumping UDPFragments" <<
			// std::endl;
			for(size_t idx = 0; idx < raw->size(); ++idx)
			{
				if(idx > 0)
				{
					outputJSON << ",";
				}
				outputJSON << "{";
				const auto& frag((*raw)[idx]);

				ots::UDPFragment bb(frag);
				int              type = 0;

				if(frag.hasMetadata())
				{
					outputJSON << "\"metadata\":{";
					// std::cout << __COUT_HDR_FL__ << "Fragment metadata: " << std::endl;
					auto md = frag.metadata<ots::UDPFragment::Metadata>();
					outputJSON << "\"port\":" << std::to_string(md->port) << ",";
					char               buf[sizeof(in_addr)];
					struct sockaddr_in addr;
					addr.sin_addr.s_addr = md->address;
					inet_ntop(AF_INET, &(addr.sin_addr), buf, INET_ADDRSTRLEN);
					outputJSON << "\"address\":\"" << std::string(buf) << "\"";
					outputJSON << "},";
				}
				outputJSON << "\"header\":{";
				outputJSON << "\"event_size\":" << std::to_string(bb.hdr_event_size()) << ",";
				outputJSON << "\"data_type\":" << std::to_string(bb.hdr_data_type());
				type = bb.hdr_data_type();
				outputJSON << "},";
				outputJSON << "\"data\":";
				if(type == 0 || type > 2)
				{
					outputJSON << "[";
					auto it = bb.dataBegin();
					outputJSON << std::hex << "\"0x" << (int)*it << "\"" << std::dec;
					++it;

					for(; it != bb.dataEnd(); ++it)
					{
						outputJSON << "," << std::hex << "\"0x" << (int)*it << "\"" << std::dec;
					}
					outputJSON << "]";
				}
				else
				{
					if(type == 2)
					{
						outputJSON << "\"";
					}
					std::string output = std::string((const char*)bb.dataBegin());
					if(type == 2)
					{
						std::string find    = "\"";
						std::string replace = "\\\"";
						for(std::string::size_type i = 0; (i = output.find(find, i)) != std::string::npos;)
						{
							output.replace(i, find.length(), replace);
							i += replace.length();
						}
					}
					outputJSON << output;
					if(type == 2)
					{
						outputJSON << "\"";
					}
				}
				outputJSON << "}";
			}
		}
		else
		{
			return;
		}

		outputJSON << "]}";
		// std::cout << __COUT_HDR_FL__ <<"JSONDispatcher filling JSON into buffer" <<
		// std::endl;
		std::string s = outputJSON.str() + "\0";
		// std::cout << __COUT_HDR_FL__ << "JSONDispatcher output: " << s << std::endl;
		auto message = boost::asio::buffer(s);
		// std::cout << __COUT_HDR_FL__ <<"JSONDispatcher broadcasting JSON data" <<
		// std::endl;
		socket_.send_to(message, remote_endpoint_);
		// std::cout << __COUT_HDR_FL__ << "JSONDispatcher done with event" << std::endl;
	}
}

DEFINE_ART_MODULE(ots::JSONDispatcher)
