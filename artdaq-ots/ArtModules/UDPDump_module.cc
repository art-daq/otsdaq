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
#include "canvas/Utilities/Exception.h"

#include "artdaq-core/Data/Fragment.hh"
#include "artdaq-ots/Overlays/UDPFragment.hh"
#include "otsdaq/Macros/CoutMacros.h"

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
class UDPDump;
}

class ots::UDPDump : public art::EDAnalyzer
{
  public:
	explicit UDPDump(fhicl::ParameterSet const& pset);
	virtual ~UDPDump();

	virtual void analyze(art::Event const& evt);

  private:
	std::string  raw_data_label_;
	std::string  frag_type_;
	unsigned int num_bytes_to_show_;
};

ots::UDPDump::UDPDump(fhicl::ParameterSet const& pset)
    : EDAnalyzer(pset)
    , raw_data_label_(pset.get<std::string>("raw_data_label"))
    , frag_type_(pset.get<std::string>("frag_type"))
    , num_bytes_to_show_(pset.get<int>("num_bytes"))
{
}

ots::UDPDump::~UDPDump() {}

void ots::UDPDump::analyze(art::Event const& evt)
{
	art::EventNumber_t eventNumber = evt.event();

	// ***********************
	// *** Toy Fragments ***
	// ***********************

	// look for raw UDP data

	art::Handle<artdaq::Fragments> raw;
	evt.getByLabel(raw_data_label_, frag_type_, raw);

	if(raw.isValid())
	{
		std::cout << __COUT_HDR_FL__ << "######################################################################" << std::endl;

		std::cout << __COUT_HDR_FL__ << std::dec << "Run " << evt.run() << ", subrun " << evt.subRun() << ", event " << eventNumber << " has " << raw->size()
		          << " fragment(s) of type " << frag_type_ << std::endl;

		for(size_t idx = 0; idx < raw->size(); ++idx)
		{
			const auto& frag((*raw)[idx]);

			ots::UDPFragment bb(frag);

			std::cout << __COUT_HDR_FL__ << "UDP fragment " << frag.fragmentID() << " has total byte count = " << bb.udp_data_words() << std::endl;

			if(frag.hasMetadata())
			{
				std::cout << __COUT_HDR_FL__ << "Fragment metadata: " << std::endl;
				ots::UDPFragment::Metadata const* md = frag.metadata<ots::UDPFragment::Metadata>();

				char               buf[sizeof(in_addr)];
				struct sockaddr_in addr;
				addr.sin_addr.s_addr = md->address;
				inet_ntop(AF_INET, &(addr.sin_addr), buf, INET_ADDRSTRLEN);

				std::cout << __COUT_HDR_FL__ << "Board port number = " << ((int)md->port) << ", address = " << std::string(buf) << std::endl;
			}

			int type = bb.hdr_data_type();
			if(type == 0 || type > 2)
			{
				auto it = bb.dataBegin();
				std::cout << __COUT_HDR_FL__ << std::hex << "0x" << (int)*it << std::endl;
				++it;

				for(; it != bb.dataEnd(); ++it)
				{
					std::cout << __COUT_HDR_FL__ << ", " << std::hex << "0x" << (int)*it << std::endl;
				}
			}
			else
			{
				std::string output = std::string((const char*)bb.dataBegin());
				std::cout << __COUT_HDR_FL__ << output << std::endl;
			}
		}
	}
	else
	{
		std::cout << __COUT_HDR_FL__ << std::dec << "Run " << evt.run() << ", subrun " << evt.subRun() << ", event " << eventNumber << " has zero"
		          << " UDP fragments." << std::endl;
	}
}

DEFINE_ART_MODULE(ots::UDPDump)
