/*--------------------------------------------------------------------
 To compile: make
 To run:
--------------------------------------------------------------------*/

#include "Tests/SimpleSoap/include/SimpleSoap.h"
#include <xdaq/NamespaceURI.h>
#include <xoap/Method.h>
#include <iostream>
#include "Utilities/MacroUtilities/include/Macro.h"
#include "Utilities/SOAPUtilities/include/SOAPUtilities.h"

using namespace ots;

XDAQ_INSTANTIATOR_IMPL(SimpleSoap)

//==============================================================================
SimpleSoap::SimpleSoap(xdaq::ApplicationStub* s) : xdaq::Application(s), SOAPMessenger(this)
{
	xgi::bind(this, &SimpleSoap::Default, "Default");
	xgi::bind(this, &SimpleSoap::StateMachineXgiHandler, "StateMachineXgiHandler");

	xoap::bind(this, &SimpleSoap::Start, "Start", XDAQ_NS_URI);
	fsm_.addState('I', "Initial", this, &SimpleSoap::stateInitial);
	fsm_.addState('H', "Halted", this, &SimpleSoap::stateHalted);
	fsm_.addStateTransition('I', 'H', "Initialize");
}

//==============================================================================
void SimpleSoap::Default(xgi::Input* in, xgi::Output* out)
{
	std::string url = "/" + getApplicationDescriptor()->getURN();
	std::cout << __COUT_HDR_FL__ << url << std::endl;
	// url = "http://131.225.82.72:1983";
	*out << cgicc::HTMLDoctype(cgicc::HTMLDoctype::eStrict) << std::endl;
	*out << cgicc::html().set("lang", "en").set("dir", "ltr") << std::endl;
	*out << cgicc::title("Simple button page") << std::endl;
	*out << "<body>" << std::endl;
	*out << "  <form name=\"input\" method=\"get\" action=\"" << url << "/StateMachineXgiHandler"
	     << "\" enctype=\"multipart/form-data\">" << std::endl;
	*out << "    <p align=\"left\"><input type=\"submit\" name=\"StateInput\" "
	        "value=\"PushStart\"/></p>"
	     << std::endl;
	*out << "  </form>" << std::endl;
	*out << "  <form name=\"input\" method=\"get\" action=\"" << url << "/Start"
	     << "\" enctype=\"multipart/form-data\">" << std::endl;
	*out << "    <p align=\"left\"><input type=\"submit\" name=\"StateInput\" "
	        "value=\"Start\"/></p>"
	     << std::endl;
	*out << "  </form>" << std::endl;
	*out << "</body>" << std::endl;
}

//==============================================================================
void SimpleSoap::StateMachineXgiHandler(xgi::Input* in, xgi::Output* out)
{
	cgicc::Cgicc cgi(in);
	std::string  Command = cgi.getElement("StateInput")->getValue();

	if(Command == "PushStart")
	{
		std::cout << __COUT_HDR_FL__ << "Got start" << std::endl;
		xoap::MessageReference msg   = SOAPUtilities::makeSOAPMessageReference("Start");
		xoap::MessageReference reply = Start(msg);

		if(receive(reply) == "StartDone")
			std::cout << __COUT_HDR_FL__ << "Everything started correctly!" << std::endl << std::endl;
		else
			std::cout << __COUT_HDR_FL__ << "All underlying Supervisors could not be started by browser button!" << std::endl << std::endl;
	}
	else if(Command == "Start")
	{
		// std::set<xdaq::ApplicationDescriptor*> set_SimpleSoap =
		// getApplicationContext()->getDefaultZone()->getApplicationGroup("rivera")->getApplicationDescriptors("SimpleSoap::SimpleSoap");
		std::set<xdaq::ApplicationDescriptor*> set_SimpleSoap =
		    getApplicationContext()->getDefaultZone()->getApplicationGroup("daq")->getApplicationDescriptors("SimpleSoap::SimpleSoap");

		for(std::set<xdaq::ApplicationDescriptor*>::iterator i_set_SimpleSoap = set_SimpleSoap.begin(); i_set_SimpleSoap != set_SimpleSoap.end();
		    ++i_set_SimpleSoap)
		{
			try
			{
				std::string sReply = send(*i_set_SimpleSoap, Command.c_str());

				if(sReply == "StartDone")
					std::cout << __COUT_HDR_FL__ << "Everything started correctly!" << std::endl << std::endl;
				else
					std::cout << __COUT_HDR_FL__
					          << "All underlying Supervisors could not be started by "
					             "browser button!"
					          << std::endl
					          << std::endl;
			}
			catch(xdaq::exception::Exception& e)
			{
				std::cout << __COUT_HDR_FL__
				          //<< std::stringF((*i_set_SimpleSoap)->getInstance())
				          << " Couldn't start sending a msg" << std::endl;
			}
		}
	}
	else
	{
		std::cout << __COUT_HDR_FL__ << "Don't understand the command: " << Command << std::endl;
	}

	this->Default(in, out);
}

//==============================================================================
xoap::MessageReference SimpleSoap::Start(xoap::MessageReference msg)
{
	std::cout << __COUT_HDR_FL__ << "Starting" << std::endl;
	return SOAPUtilities::makeSOAPMessageReference("StartDone");
}

//==============================================================================
void SimpleSoap::stateInitial(toolbox::fsm::FiniteStateMachine& fsm)
{
	std::cout << __COUT_HDR_FL__ << "--- SimpleWeb is in its Initial state ---" << std::endl;
	state_ = fsm_.getStateName(fsm_.getCurrentState());

	// diagService_->reportError("PixelSupervisor::stateInitial: workloop active:
	// "+stringF(calibWorkloop_->isActive())+", workloop type:
	// "+calibWorkloop_->getType(),DIAGINFO);
}

//==============================================================================
void SimpleSoap::stateHalted(toolbox::fsm::FiniteStateMachine& fsm)
{
	std::cout << __COUT_HDR_FL__ << "--- SimpleWeb is in its Halted state ---" << std::endl;
	state_ = fsm_.getStateName(fsm_.getCurrentState());
}
