#include "otsdaq/DataManager/DataManager.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/DataManager/CircularBuffer.h"
#include "otsdaq/DataManager/DataConsumer.h"
#include "otsdaq/DataManager/DataProducerBase.h"
#include "otsdaq/DataManager/MakeDataProcessor.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <unistd.h>  //usleep
#include <iostream>
#include <vector>

using namespace ots;

// clang-format off
//==============================================================================
DataManager::DataManager(const ConfigurationTree& theXDAQContextConfigTree, const std::string& supervisorConfigurationPath)
    : Configurable(theXDAQContextConfigTree, supervisorConfigurationPath)
	, VStateMachine(Configurable::theConfigurationRecordName_)
	, parentSupervisorHasFrontends_(false)
{
	__CFG_COUT__ << "Constructed." << __E__;
}  // end constructor
// clang-format on

//==============================================================================
DataManager::~DataManager(void)
{
	__CFG_COUT__ << "Destructor." << __E__;
	DataManager::destroyBuffers();
	__CFG_COUT__ << "Destructed." << __E__;
}  // end destructor

//==============================================================================
void DataManager::dumpStatus(std::ostream* out) const
{
	*out << "Buffer count: " << buffers_.size() << __E__;
	for(auto& bufferPair : buffers_)
	{
		*out << "\t"
		     << "Buffer '" << bufferPair.first << "' status=" << bufferPair.second.status_ << " producers=" << bufferPair.second.producers_.size()
		     << " consumers=" << bufferPair.second.consumers_.size() << __E__;

		*out << "\t\t"
		     << "Producers:" << __E__;
		for(auto& producer : bufferPair.second.producers_)
		{
			*out << "\t\t\t" << producer->getProcessorID() << " [" << bufferPair.second.buffer_->getProducerBufferSize(producer->getProcessorID()) << "]"
			     << __E__;
		}
		*out << "\t\t"
		     << "Consumers:" << __E__;
		for(auto& consumer : bufferPair.second.consumers_)
		{
			*out << "\t\t\t" << consumer->getProcessorID() << __E__;
		}
	}
}  // end dumpStatus()

//==============================================================================
void DataManager::configure(void)
{
	const std::string transitionName = "Configuring";

	const std::string COL_NAME_bufferGroupLink    = "LinkToDataBufferTable";
	const std::string COL_NAME_processorGroupLink = "LinkToDataProcessorTable";
	const std::string COL_NAME_processorType      = "ProcessorType";
	const std::string COL_NAME_processorPlugin    = "ProcessorPluginName";
	const std::string COL_NAME_processorLink      = "LinkToProcessorTable";
	const std::string COL_NAME_appUID             = "ApplicationUID";

	__CFG_COUT__ << transitionName << " DataManager" << __E__;
	__CFG_COUT__ << "Path: " << theConfigurationPath_ + "/" + COL_NAME_bufferGroupLink << __E__;

	destroyBuffers();

	// get all buffer definitions from configuration tree
	for(const auto& buffer :
	    theXDAQContextConfigTree_.getNode(theConfigurationPath_ + "/" + COL_NAME_bufferGroupLink).getChildren())  //"/LinkToDataManagerTable").getChildren())
	{
		__CFG_COUT__ << "Data Buffer Name: " << buffer.first << __E__;
		if(buffer.second.getNode(TableViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
		{
			std::vector<unsigned int> producersVectorLocation;
			std::vector<unsigned int> consumersVectorLocation;
			auto         bufferConfigurationList = buffer.second.getNode(COL_NAME_processorGroupLink).getChildren();  //"LinkToDataBufferTable").getChildren();
			unsigned int location                = 0;
			for(const auto& bufferConfiguration : bufferConfigurationList)
			{
				__CFG_COUT__ << "Processor id: " << bufferConfiguration.first << __E__;
				if(bufferConfiguration.second.getNode(TableViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
				{
					if(bufferConfiguration.second.getNode(COL_NAME_processorType).getValue<std::string>() == "Producer")
					{
						producersVectorLocation.push_back(location);
					}
					else if(bufferConfiguration.second.getNode(COL_NAME_processorType).getValue<std::string>() == "Consumer")
					{
						consumersVectorLocation.push_back(location);
					}
					else
					{
						__CFG_SS__ << "Node ProcessorType in " << bufferConfiguration.first << " of type "
						           << bufferConfiguration.second.getNode(COL_NAME_processorPlugin).getValue<std::string>()
						           << " is invalid. The only accepted types are Producer "
						              "and Consumer"
						           << __E__;
						__CFG_MOUT_ERR__ << ss.str();
						__CFG_SS_THROW__;
					}
				}
				++location;

			}  // end loop sorting by producer and consumer

			if(!parentSupervisorHasFrontends_ && producersVectorLocation.size() == 0)  // || consumersVectorLocation.size() == 0)
			{
				__CFG_SS__ << "Node Data Buffer " << buffer.first << " has " << producersVectorLocation.size() << " Producers"
				           << " and " << consumersVectorLocation.size() << " Consumers"
				           << " there must be at least 1 Producer " <<  //	of both configured
				    "for the buffer!" << __E__;
				__CFG_MOUT_ERR__ << ss.str();
				__CFG_SS_THROW__;
			}

			if(parentSupervisorHasFrontends_)
				__CFG_COUT__ << "Parent supervisor has front-ends, so FE-producers may "
				             << "be instantiated in the configure steps of the FESupervisor." << __E__;

			configureBuffer<std::string, std::map<std::string, std::string> >(buffer.first);

			for(auto& producerLocation : producersVectorLocation)
			{
				//				__CFG_COUT__ << theConfigurationPath_ << __E__;
				//				__CFG_COUT__ << buffer.first << __E__;
				__CFG_COUT__ << "Creating producer... " << bufferConfigurationList[producerLocation].first << __E__;
				//				__CFG_COUT__ <<
				// bufferConfigurationMap[producer].getNode(COL_NAME_processorPlugin).getValue<std::string>()
				//<< __E__;
				//				__CFG_COUT__ <<
				// bufferConfigurationMap[producer].getNode("LinkToProcessorTable") <<
				// __E__;
				//				__CFG_COUT__ << "THIS DATA MANAGER POINTER: " << this <<
				// __E__;
				//				__CFG_COUT__ << "PASSED" << __E__;

				try
				{
					// buffers_[buffer.first].producers_.push_back(std::shared_ptr<DataProducerBase>(
					DataProducerBase* tmpCastCheck = dynamic_cast<DataProducerBase*>(
					    makeDataProcessor(bufferConfigurationList[producerLocation].second.getNode(COL_NAME_processorPlugin).getValue<std::string>(),
					                      theXDAQContextConfigTree_.getBackNode(theConfigurationPath_).getNode(COL_NAME_appUID).getValue<std::string>(),
					                      buffer.first,
					                      bufferConfigurationList[producerLocation].first,
					                      theXDAQContextConfigTree_,
					                      theConfigurationPath_ + "/" + COL_NAME_bufferGroupLink + "/" + buffer.first + "/" + COL_NAME_processorGroupLink +
					                          "/" + bufferConfigurationList[producerLocation].first + "/" + COL_NAME_processorLink));  //));

					if(!tmpCastCheck)
					{
						__CFG_SS__ << "Construction failed for producer '" << bufferConfigurationList[producerLocation].first << "!' Null pointer returned."
						           << __E__;
						__CFG_SS_THROW__;
					}
					__CFG_COUT__ << tmpCastCheck->getProcessorID() << __E__;

					{
						__CFG_SS__;
						dumpStatus((std::ostream*)&ss);
						__CFG_COUT__ << ss.str() << __E__;
					}
				}
				catch(const std::bad_cast& e)
				{
					__CFG_SS__ << "Failed to instantiate producer plugin named '" << bufferConfigurationList[producerLocation].first << "' of type '"
					           << bufferConfigurationList[producerLocation].second.getNode(COL_NAME_processorPlugin).getValue<std::string>()
					           << "' due to the following error: \n"
					           << e.what() << __E__;
					__CFG_MOUT_ERR__ << ss.str();
					__CFG_SS_THROW__;
				}
				catch(const cet::exception& e)
				{
					__CFG_SS__ << "Failed to instantiate producer plugin named '" << bufferConfigurationList[producerLocation].first << "' of type '"
					           << bufferConfigurationList[producerLocation].second.getNode(COL_NAME_processorPlugin).getValue<std::string>()
					           << "' due to the following error: \n"
					           << e.what() << __E__;
					__CFG_MOUT_ERR__ << ss.str();
					__CFG_SS_THROW__;
				}
				catch(const std::runtime_error& e)
				{
					__CFG_SS__ << "Failed to instantiate producer plugin named '" << bufferConfigurationList[producerLocation].first << "' of type '"
					           << bufferConfigurationList[producerLocation].second.getNode(COL_NAME_processorPlugin).getValue<std::string>()
					           << "' due to the following error: \n"
					           << e.what() << __E__;
					__CFG_MOUT_ERR__ << ss.str();
					__CFG_SS_THROW__;
				}
				catch(...)
				{
					__CFG_SS__ << "Failed to instantiate producer plugin named '" << bufferConfigurationList[producerLocation].first << "' of type '"
					           << bufferConfigurationList[producerLocation].second.getNode(COL_NAME_processorPlugin).getValue<std::string>()
					           << "' due to an unknown error." << __E__;
					try	{ throw; } //one more try to printout extra info
					catch(const std::exception &e)
					{
						ss << "Exception message: " << e.what();
					}
					catch(...){}
					__CFG_MOUT_ERR__ << ss.str();
					throw;  // if we do not throw, it is hard to tell what is causing the
					        // problem..
					        //__CFG_SS_THROW__;
				}
				__CFG_COUT__ << bufferConfigurationList[producerLocation].first << " has been created!" << __E__;
			}  // end producer creation loop

			for(auto& consumerLocation : consumersVectorLocation)
			{
				//				__CFG_COUT__ << theConfigurationPath_ << __E__;
				//				__CFG_COUT__ << buffer.first << __E__;
				__CFG_COUT__ << "Creating consumer... " << bufferConfigurationList[consumerLocation].first << __E__;
				//				__CFG_COUT__ <<
				// bufferConfigurationMap[consumer].getNode(COL_NAME_processorPlugin).getValue<std::string>()
				//<< __E__;
				//				__CFG_COUT__ <<
				// bufferConfigurationMap[consumer].getNode("LinkToProcessorTable") <<
				// __E__;
				//				__CFG_COUT__ <<
				// theXDAQContextConfigTree_.getBackNode(theConfigurationPath_) <<
				// __E__;
				//				__CFG_COUT__ << "THIS DATA MANAGER POINTER: " << this <<
				// __E__;
				//				__CFG_COUT__ << "PASSED" << __E__;
				try
				{
					// buffers_[buffer.first].consumers_.push_back(std::shared_ptr<DataConsumer>(
					DataConsumer* tmpCastCheck = dynamic_cast<DataConsumer*>(
					    makeDataProcessor(bufferConfigurationList[consumerLocation].second.getNode(COL_NAME_processorPlugin).getValue<std::string>(),
					                      theXDAQContextConfigTree_.getBackNode(theConfigurationPath_).getNode(COL_NAME_appUID).getValue<std::string>(),
					                      buffer.first,
					                      bufferConfigurationList[consumerLocation].first,
					                      theXDAQContextConfigTree_,
					                      theConfigurationPath_ + "/" + COL_NAME_bufferGroupLink + "/" + buffer.first + "/" + COL_NAME_processorGroupLink +
					                          "/" + bufferConfigurationList[consumerLocation].first + "/" + COL_NAME_processorLink));  //));

					if(!tmpCastCheck)
					{
						__CFG_SS__ << "Construction failed for consumer '" << bufferConfigurationList[consumerLocation].first << "!' Null pointer returned."
						           << __E__;
						__CFG_SS_THROW__;
					}

					{
						__CFG_SS__;
						dumpStatus((std::ostream*)&ss);
						__CFG_COUT__ << ss.str() << __E__;
					}
				}
				catch(const std::bad_cast& e)
				{
					__CFG_SS__ << "Failed to instantiate consumer plugin named '" << bufferConfigurationList[consumerLocation].first << "' of type '"
					           << bufferConfigurationList[consumerLocation].second.getNode(COL_NAME_processorPlugin).getValue<std::string>()
					           << "' due to the following error: \n"
					           << e.what() << __E__;
					__CFG_MOUT_ERR__ << ss.str();
					__CFG_SS_THROW__;
				}
				catch(const cet::exception& e)
				{
					__CFG_SS__ << "Failed to instantiate consumer plugin named '" << bufferConfigurationList[consumerLocation].first << "' of type '"
					           << bufferConfigurationList[consumerLocation].second.getNode(COL_NAME_processorPlugin).getValue<std::string>()
					           << "' due to the following error: \n"
					           << e.what() << __E__;
					__CFG_MOUT_ERR__ << ss.str();
					__CFG_SS_THROW__;
				}
				catch(const std::runtime_error& e)
				{
					__CFG_SS__ << "Failed to instantiate consumer plugin named '" << bufferConfigurationList[consumerLocation].first << "' of type '"
					           << bufferConfigurationList[consumerLocation].second.getNode(COL_NAME_processorPlugin).getValue<std::string>()
					           << "' due to the following error: \n"
					           << e.what() << __E__;
					__CFG_MOUT_ERR__ << ss.str();
					__CFG_SS_THROW__;
				}
				catch(...)
				{
					__CFG_SS__ << "Failed to instantiate consumer plugin named '" << bufferConfigurationList[consumerLocation].first << "' of type '"
					           << bufferConfigurationList[consumerLocation].second.getNode(COL_NAME_processorPlugin).getValue<std::string>()
					           << "' due to an unknown error." << __E__;
					try	{ throw; } //one more try to printout extra info
					catch(const std::exception &e)
					{
						ss << "Exception message: " << e.what();
					}
					catch(...){}
					__CFG_MOUT_ERR__ << ss.str();
					throw;  // if we do not throw, it is hard to tell what is happening..
					        //__CFG_SS_THROW__;
				}
				__CFG_COUT__ << bufferConfigurationList[consumerLocation].first << " has been created!" << __E__;
			}  // end consumer creation loop
		}
	}
	DataManager::configureAllBuffers();
}  // end configure()

//==============================================================================
void DataManager::halt(void)
{
	const std::string transitionName = "Halting";

	__CFG_COUT__ << transitionName << " DataManager " << __E__;

	// since halting also occurs on errors, ignore more errors
	try
	{
		stop();
	}
	catch(...)
	{
		__CFG_COUT_WARN__ << "An error occurred while halting the Data Manager, ignoring." << __E__;
	}

	stop();

	__CFG_COUT__ << transitionName << " DataManager stopped. Now destruct buffers..." << __E__;

	DataManager::destroyBuffers();  // Stop all Buffers, deletes all pointers, and delete
	                                // Buffer struct
}  // end halt()

//==============================================================================
void DataManager::pause(void)
{
	const std::string transitionName = "Pausing";

	__CFG_COUT__ << transitionName << " DataManager " << __E__;

	DataManager::pauseAllBuffers();
}  // end pause()

//==============================================================================
void DataManager::resume(void)
{
	const std::string transitionName = "Resuming";

	__CFG_COUT__ << transitionName << " DataManager " << __E__;

	DataManager::resumeAllBuffers();
}  // end resume()

//==============================================================================
void DataManager::start(std::string runNumber)
{
	const std::string transitionName = "Starting";

	__CFG_COUT__ << transitionName << " DataManager " << __E__;

	DataManager::startAllBuffers(runNumber);
}  // end start()

//==============================================================================
void DataManager::stop()
{
	const std::string transitionName = "Stopping";

	__CFG_COUT__ << transitionName << " DataManager " << __E__;

	DataManager::stopAllBuffers();
}  // end stop()

//==============================================================================
// destroyBuffers
//	Stop all Buffers, deletes all pointers, and delete Buffer struct
void DataManager::destroyBuffers(void)
{
	DataManager::stopAllBuffers();

	for(auto& bufferPair : buffers_)
	{
		// delete all producers/consumers
		// then delete CircularBuffer
		for(auto& producer : bufferPair.second.producers_)
			delete producer;
		bufferPair.second.producers_.clear();

		for(auto& consumer : bufferPair.second.consumers_)
			delete consumer;
		bufferPair.second.consumers_.clear();

		delete bufferPair.second.buffer_;
	}  // end delete buffer loop

	buffers_.clear();
}  // end destroyBuffers()

////==============================================================================
// void DataManager::eraseBuffer(const std::string& bufferUID)
//{
//	if (deleteBuffer(bufferUID))
//		buffers_.erase(bufferUID);
//} //end eraseBuffer()
//
////==============================================================================
//
// bool DataManager::deleteBuffer(const std::string& bufferUID)
//{
//	auto it = buffers_.find(bufferUID);
//	if (it != buffers_.end())
//	{
//		auto aBuffer = it->second;
//		if (aBuffer.status_ == Running)
//			stopBuffer(bufferUID);
//
////		for (auto& itc : aBuffer.consumers_)
////		{
////			aBuffer.buffer_->unregisterConsumer(itc.get());
////			//			delete itc;
////		}
//		aBuffer.consumers_.clear();
//		//		for(auto& itp: aBuffer.producers_)
//		//			delete itp;
//		aBuffer.producers_.clear();
//
//		delete aBuffer.buffer_;
//		return true;
//	}
//	return false;
//} //end deleteBuffer()
//
////==============================================================================
////unregisterConsumer
////	Unregister consumer from its DataBuffer buffers. And destroy the processor!
////	Assumes that the DataManager has ownership of processor object.
////
////	Note: there should only be one DataBuffer with that consumer registered!
// void DataManager::unregisterConsumer(const std::string& bufferID, const std::string&
// consumerID)
//{
//	__CFG_COUT__ << "Un-Registering consumer '" << consumerID <<
//			"' from buffer '" << bufferID << "'..." << __E__;
//
//	auto bufferIt = buffers_.find(bufferID);
//	if(bufferIt == buffers_.end())
//	{
//		__CFG_SS__ << "While Un-Registering consumer '" << consumerID <<
//				",' buffer '" << bufferID << "' not found!" << __E__;
//		__CFG_SS_THROW__;
//	}
//
//	//just destroy consumer, and it unregisters itself
//	for (auto consumerIt = bufferIt->second.consumers_.begin();
//			consumerIt != bufferIt->second.consumers_.end(); consumerIt++)
//	{
//		if((*consumerIt)->getProcessorID() == consumerID)
//		{
//			bufferIt->second.consumers_.erase(consumerIt);
//			break;
//		}
//	}
//	//DO NOT DO ANY STRING BASED UNREGISTERING.. leave it to end of DataManager halt
//	//bufferIt->second.buffer_->unregisterConsumer(consumerID);
//
//	__CFG_COUT__ << "Un-Registered consumer '" << consumerID <<
//			"' from buffer '" << bufferID << ".'" << __E__;
//	{__CFG_SS__; dumpStatus((std::ostream*)&ss); __CFG_COUT__ << ss.str() << __E__;}
////
////	__CFG_COUT__ << "Un-Registering consumer '" << consumerID <<
////			"'..." << __E__;
////
////	for (auto& buffer: buffers_)
////	{
////		try
////		{
////
////			//just destroy consumer, and it unregisters itself
////
////
////
////			//remove from circular buffer and from consumer vector
////
////			//remove from circular buffer
////			//buffer.second.buffer_->unregisterConsumer(consumerID);
////
////			//remove from consumer vector
////			for (auto& consumer: consumers_)
////			{
////				if(consumer->processorUID_ == consumerID)
////				{
////					buffer.second.consumers_.erase(consumer);
////					break;
////				}
////			}
////
////			__CFG_COUT__ << "Un-Registered consumer '" << consumerID <<
////					".'" << __E__;
////			{__CFG_SS__; dumpStatus((std::ostream*)&ss); __CFG_COUT__ << ss.str() <<
///__E__;}
////
////			return;
////		}
////		catch(...)
////		{} //ignore not found
////	}
//
////	RAR commented.. this was bypassing unregister at CircularBuffer level
////	for (auto it = buffers_.begin(); it != buffers_.end(); it++)
////		for (auto& itc : it->second.consumers_)
////		{
////			if (itc->getProcessorID() == consumerID)
////			{
////				it->second.buffer_->unregisterConsumer(itc.get());
////
////				{__CFG_SS__; dumpStatus((std::ostream*)&ss); __CFG_COUT__ << ss.str()
///<<
///__E__;}
////
////				return;
////			}
////		}
//
//	__CFG_SS__ << "While Un-Registering, consumer '" << consumerID <<
//			"' not found!" << __E__;
//	__CFG_SS_THROW__;
//} //end unregisterConsumer()
//
////==============================================================================
// void DataManager::unregisterProducer(const std::string& bufferID, const std::string&
// producerID)
//{
//	__CFG_COUT__ << "Un-Registering producer '" << producerID <<
//			"' from buffer '" << bufferID << "'..." << __E__;
//
//	auto bufferIt = buffers_.find(bufferID);
//	if(bufferIt == buffers_.end())
//	{
//		__CFG_SS__ << "While Un-Registering producer '" << producerID <<
//				",' buffer '" << bufferID << "' not found!" << __E__;
//		__CFG_SS_THROW__;
//	}
//
//	//DO NOT DO ANY STRING BASED UNREGISTERING.. leave it to end of DataManager halt
//	//bufferIt->second.unregisterProducer(producerID);
//
//	__CFG_COUT__ << "Un-Registered producer '" << producerID <<
//			"' from buffer '" << bufferID << ".'" << __E__;
//	{__CFG_SS__; dumpStatus((std::ostream*)&ss); __CFG_COUT__ << ss.str() << __E__;}
//} //end unregisterProducer()

//==============================================================================
void DataManager::unregisterFEProducer(const std::string& bufferID, const std::string& feProducerID)
{
	__CFG_COUT__ << "Un-Registering FE-producer '" << feProducerID << "' from buffer '" << bufferID << "'..." << __E__;

	auto bufferIt = buffers_.find(bufferID);
	if(bufferIt == buffers_.end())
	{
		__CFG_SS__ << "While Un-Registering FE-producer '" << feProducerID << ",' buffer '" << bufferID << "' not found!" << __E__;
		__CFG_SS_THROW__;
	}

	// DO NOT DO ANY STRING BASED UNREGISTERING.. leave it to end of DataManager halt
	// DO NOT DO bufferIt->second.unregisterProducer(producerID);

	// remove from producer vector
	// just destroy consumer, and it unregisters itself
	for(auto feProducerIt = bufferIt->second.producers_.begin(); feProducerIt != bufferIt->second.producers_.end(); feProducerIt++)
	{
		if((*feProducerIt)->getProcessorID() == feProducerID)
		{
			// do not delete pointer before erasing
			// because FEVInterfacesManager will delete FEProducer instance
			bufferIt->second.producers_.erase(feProducerIt);
			break;
		}
	}

	__CFG_COUT__ << "Un-Registered FE-producer '" << feProducerID << "' from buffer '" << bufferID << ".'" << __E__;
	{
		__CFG_SS__;
		dumpStatus((std::ostream*)&ss);
		__CFG_COUT__ << ss.str() << __E__;
	}

}  // end unregisterFEProducer()

//==============================================================================
// registerProducer
//	DataManager takes ownership of producer pointer
//		and is now responsible for destructing.
//	Note: in the future, we could pass a shared_ptr, so that source of pointer could
//		share in destructing responsibility.
void DataManager::registerProducer(const std::string& bufferUID, DataProducerBase* producer)
{
	__CFG_COUT__ << "Registering producer '" << producer->getProcessorID() << "' to buffer '" << bufferUID << "'..." << __E__;

	auto bufferIt = buffers_.find(bufferUID);
	if(bufferIt == buffers_.end())
	{
		__CFG_SS__ << "Can't find buffer UID '" + bufferUID << "' for producer '" << producer->getProcessorID()
		           << ".' Make sure that your configuration is correct!" << __E__;

		ss << "\n\n Here is the list of buffers:" << __E__;
		for(const auto& bufferPair : buffers_)
			ss << bufferPair.first << __E__;
		ss << "\n\n";

		__CFG_SS_THROW__;
	}

	{
		__CFG_SS__ << "Before!" << __E__;
		dumpStatus((std::ostream*)&ss);
		__CFG_COUT__ << ss.str() << __E__;
	}

	__CFG_COUTV__(producer->getBufferSize());
	bufferIt->second.buffer_->registerProducer(producer, producer->getBufferSize());
	bufferIt->second.producers_.push_back(producer);  // this is where ownership is taken!

	{
		__CFG_SS__ << "After!" << __E__;
		dumpStatus((std::ostream*)&ss);
		__CFG_COUT__ << ss.str() << __E__;
	}
}

//==============================================================================
void DataManager::registerConsumer(const std::string& bufferUID, DataConsumer* consumer)
{
	__CFG_COUT__ << "Registering consumer '" << consumer->getProcessorID() << "' to buffer '" << bufferUID << "'..." << __E__;

	auto bufferIt = buffers_.find(bufferUID);
	if(bufferIt == buffers_.end())
	{
		__CFG_SS__ << "Can't find buffer UID '" + bufferUID << "' for consumer '" << consumer->getProcessorID()
		           << ".' Make sure that your configuration is correct!" << __E__;

		ss << "\n\n Here is the list of buffers:" << __E__;
		for(const auto& bufferPair : buffers_)
			ss << bufferPair.first << __E__;

		__CFG_SS_THROW__;
	}

	{
		__CFG_SS__ << "Before!" << __E__;
		dumpStatus((std::ostream*)&ss);
		__CFG_COUT__ << ss.str() << __E__;
	}

	bufferIt->second.buffer_->registerConsumer(consumer);
	bufferIt->second.consumers_.push_back(consumer);  // this is where ownership is taken!

	{
		__CFG_SS__ << "After!" << __E__;
		dumpStatus((std::ostream*)&ss);
		__CFG_COUT__ << ss.str() << __E__;
	}
}

//==============================================================================
void DataManager::configureAllBuffers(void)
{
	for(auto it = buffers_.begin(); it != buffers_.end(); it++)
		configureBuffer(it->first);
}

//==============================================================================
void DataManager::startAllBuffers(const std::string& runNumber)
{
	for(auto it = buffers_.begin(); it != buffers_.end(); it++)
		startBuffer(it->first, runNumber);
}

//==============================================================================
void DataManager::stopAllBuffers(void)
{
	for(auto it = buffers_.begin(); it != buffers_.end(); it++)
		stopBuffer(it->first);
}

//==============================================================================
void DataManager::resumeAllBuffers(void)
{
	for(auto it = buffers_.begin(); it != buffers_.end(); it++)
		resumeBuffer(it->first);
}

//==============================================================================
void DataManager::pauseAllBuffers(void)
{
	for(auto it = buffers_.begin(); it != buffers_.end(); it++)
		pauseBuffer(it->first);
}

//==============================================================================
void DataManager::configureBuffer(const std::string& bufferUID)
{
	__CFG_COUT__ << "Configuring... " << bufferUID << __E__;

	for(auto& it : buffers_[bufferUID].consumers_)
	{
		// use try..catch to make sure there is some identifying trail for errors
		try
		{
			it->configure();
		}
		catch(...)
		{
			__CFG_COUT_WARN__ << "An error occurred while configuring consumer '" << it->getProcessorID() << "'..." << __E__;
			throw;
		}
	}

	for(auto& it : buffers_[bufferUID].producers_)
	{
		// use try..catch to make sure there is some identifying trail for errors
		try
		{
			it->configure();
		}
		catch(...)
		{
			__CFG_COUT_WARN__ << "An error occurred while starting producer '" << it->getProcessorID() << "'..." << __E__;
			throw;
		}
	}

	buffers_[bufferUID].status_ = Initialized;

}  // end startBuffer()

//==============================================================================
void DataManager::startBuffer(const std::string& bufferUID, std::string runNumber)
{
	__CFG_COUT__ << "Starting... " << bufferUID << __E__;

	buffers_[bufferUID].buffer_->reset();
	for(auto& it : buffers_[bufferUID].consumers_)
	{
		// use try..catch to make sure there is some identifying trail for errors
		try
		{
			it->startProcessingData(runNumber);
		}
		catch(...)
		{
			__CFG_COUT_WARN__ << "An error occurred while starting consumer '" << it->getProcessorID() << "'..." << __E__;
			throw;
		}
	}

	for(auto& it : buffers_[bufferUID].producers_)
	{
		// use try..catch to make sure there is some identifying trail for errors
		try
		{
			it->startProcessingData(runNumber);
		}
		catch(...)
		{
			__CFG_COUT_WARN__ << "An error occurred while starting producer '" << it->getProcessorID() << "'..." << __E__;
			throw;
		}
	}

	buffers_[bufferUID].status_ = Running;

}  // end startBuffer()

//==============================================================================
void DataManager::stopBuffer(const std::string& bufferUID)
{
	__CFG_COUT__ << "Stopping... " << bufferUID << __E__;

	__CFG_COUT__ << "Stopping producers..." << __E__;
	for(auto& it : buffers_[bufferUID].producers_)
	{
		// use try..catch to make sure there is some identifying trail for errors
		try
		{
			it->stopProcessingData();
		}
		catch(...)
		{
			__CFG_COUT_WARN__ << "An error occurred while stopping producer '" << it->getProcessorID() << "'..." << __E__;
			throw;
		}
	}

	// Wait until all buffers are flushed
	unsigned int       timeOut        = 0;
	const unsigned int ratio          = 100;
	const unsigned int sleepTime      = 1000 * ratio;
	unsigned int       totalSleepTime = sleepTime / ratio * buffers_[bufferUID].buffer_->getTotalNumberOfSubBuffers();  // 1 milliseconds for each buffer!!!!
	if(totalSleepTime < 5000000)
		totalSleepTime = 5000000;  // At least 5 seconds
	while(!buffers_[bufferUID].buffer_->isEmpty())
	{
		usleep(sleepTime);
		timeOut += sleepTime;
		if(timeOut > totalSleepTime)
		{
			__CFG_COUT__ << "Couldn't flush all buffers! Timing out after " << totalSleepTime / 1000000. << " seconds!" << __E__;
			buffers_[bufferUID].buffer_->isEmpty();
			break;
		}
	}
	__CFG_COUT__ << "Stopping consumers, buffer MUST BE EMPTY. Is buffer empty? " << (buffers_[bufferUID].buffer_->isEmpty() ? "yes" : "no") << __E__;

	for(auto& it : buffers_[bufferUID].consumers_)
	{
		// use try..catch to make sure there is some identifying trail for errors
		try
		{
			it->stopProcessingData();
		}
		catch(...)
		{
			__CFG_COUT_WARN__ << "An error occurred while stopping consumer '" << it->getProcessorID() << "'..." << __E__;
			throw;
		}
	}

	buffers_[bufferUID].buffer_->reset();
	buffers_[bufferUID].status_ = Initialized;
}  // end stopBuffer()

//==============================================================================
void DataManager::resumeBuffer(const std::string& bufferUID)
{
	__CFG_COUT__ << "Resuming... " << bufferUID << __E__;

	for(auto& it : buffers_[bufferUID].consumers_)
		it->resumeProcessingData();
	for(auto& it : buffers_[bufferUID].producers_)
		it->resumeProcessingData();

	buffers_[bufferUID].status_ = Running;
}  // end resumeBuffer()

//==============================================================================
void DataManager::pauseBuffer(const std::string& bufferUID)
{
	__CFG_COUT__ << "Pausing... " << bufferUID << __E__;

	for(auto& it : buffers_[bufferUID].producers_)
		it->pauseProcessingData();
	// Wait until all buffers are flushed
	unsigned int       timeOut   = 0;
	const unsigned int sleepTime = 1000;
	while(!buffers_[bufferUID].buffer_->isEmpty())
	{
		usleep(sleepTime);
		timeOut += sleepTime;
		if(timeOut > sleepTime * buffers_[bufferUID].buffer_->getTotalNumberOfSubBuffers())  // 1
		                                                                                     // milliseconds
		                                                                                     // for each
		                                                                                     // buffer!!!!
		{
			__CFG_COUT__ << "Couldn't flush all buffers! Timing out after " << buffers_[bufferUID].buffer_->getTotalNumberOfSubBuffers() * sleepTime / 1000000.
			             << " seconds!" << __E__;
			break;
		}
	}
	for(auto& it : buffers_[bufferUID].consumers_)
		it->pauseProcessingData();
	buffers_[bufferUID].status_ = Initialized;
}  // end pauseBuffer()
