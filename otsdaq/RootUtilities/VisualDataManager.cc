#include "otsdaq/RootUtilities/VisualDataManager.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/DataManager/DQMHistosConsumerBase.h"
#include "otsdaq/DataManager/DataManager.h"
#include "otsdaq/DataManager/DataProcessor.h"
#include "otsdaq/Macros/CoutMacros.h"

#include <TFile.h>
#include <cassert>
#include <chrono>  // std::chrono::seconds
#include <iostream>
#include <sstream>
#include <thread>  // std::this_thread::sleep_for

using namespace ots;
#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "VisualDataManager"
#define mfSubject_ (std::string("VisualDataManager"))

//==============================================================================
VisualDataManager::VisualDataManager(const ConfigurationTree& theXDAQContextConfigTree, const std::string& supervisorConfigurationPath)
    : DataManager(theXDAQContextConfigTree, supervisorConfigurationPath), doNotStop_(false), ready_(false), theLiveDQMHistos_(false)
//, theFileDQMHistos_ (supervisorType, supervisorInstance, "VisualBuffer",
//"FileDQMHistos") , theFileDQMHistos_ (supervisorType, supervisorInstance,
//"VisualBuffer", "FileDQMHistos",0) , theFileDQMHistos_ ()
{
}

//==============================================================================
VisualDataManager::~VisualDataManager(void) {}

//==============================================================================
void VisualDataManager::configure(void)
{
	fileMap_.clear();
	theLiveDQMs_.clear();
	DataManager::configure();
	auto buffers = theXDAQContextConfigTree_.getNode(theConfigurationPath_ + "/LinkToDataBufferTable").getChildren();

	__CFG_COUT__ << "Buffer count " << buffers.size() << __E__;

	for(const auto& buffer : buffers)
	{
		__CFG_COUT__ << "Data Buffer Name: " << buffer.first << std::endl;
		if(buffer.second.getNode(TableViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
		{
			std::vector<std::string> producers;
			std::vector<std::string> consumers;
			auto                     bufferConfigurationMap = buffer.second.getNode("LinkToDataProcessorTable").getChildren();
			for(const auto& bufferConfiguration : bufferConfigurationMap)
			{
				__CFG_COUT__ << "Processor id: " << bufferConfiguration.first << std::endl;
				if(bufferConfiguration.second.getNode(TableViewColumnInfo::COL_NAME_STATUS).getValue<bool>() &&
				   (bufferConfiguration.second.getNode("ProcessorType").getValue<std::string>() == "Consumer"))
				{
					__CFG_COUT__ << "Consumer Plugin Type = " << bufferConfiguration.second.getNode("ProcessorPluginName") << __E__;

					auto bufferIt = buffers_.at(buffer.first);
					for(const auto& consumer : bufferIt.consumers_)
					{
						__CFG_COUT__ << "CONSUMER PROCESSOR: " << consumer->getProcessorID() << std::endl;
						if(consumer->getProcessorID() == bufferConfiguration.second.getNode("ProcessorUID").getValue<std::string>())
						{
							__CFG_COUT__ << "CONSUMER: " << consumer->getProcessorID() << std::endl;

							try
							{
								__CFG_COUT__ << "Trying for DQMHistosConsumerBase." << __E__;
								// theLiveDQMHistos_ =
								//     dynamic_cast<DQMHistosConsumerBase*>(consumer);
								theLiveDQMs_.emplace_back(dynamic_cast<DQMHistosBase*>(consumer));
								dynamic_cast<DQMHistosBase*>(consumer)->setDataManager(this);

								// __CFG_COUT__ << "Did we succeed? " << theLiveDQMHistos_
								//              << __E__;
							}
							catch(...)
							{
							}  // ignore failures

							if(theLiveDQMs_.size() == 0)
							{
								__CFG_SS__
								    << "There are no configured visualizer consumer! There must be at least one consumer if you want to use the visualizer."
								    << __E__;
								__CFG_SS_THROW__;
							}
						}
					}
				}
			}
		}
	}
}

//==============================================================================
void VisualDataManager::halt(void)
{
	ready_            = false;
	theLiveDQMHistos_ = false;
	while(doNotStop_) {};
	DataManager::halt();
}

//==============================================================================
void VisualDataManager::pause(void)
{
	__CFG_COUT__ << "Pausing..." << std::endl;
	DataManager::pause();
}

//==============================================================================
void VisualDataManager::resume(void) { DataManager::resume(); }

//==============================================================================
void VisualDataManager::start(std::string runNumber)
{
	__CFG_COUT__ << "Start!" << __E__;

	ready_ = false;

	DataManager::start(runNumber);
	theLiveDQMHistos_ = true;

	ready_ = true;
}

//==============================================================================
void VisualDataManager::stop(void)
{
	ready_            = false;
	theLiveDQMHistos_ = false;
	while(doNotStop_) {};
	DataManager::stop();
}

//==============================================================================
void VisualDataManager::load(std::string fileName, std::string type)
{
	if(type == "Histograms")
		theFileDQMHistos_.load(fileName);
	//	else if(type == "Monicelli")
	//		theMonicelliEventAnalyzer_.load(fileName);
	//	else if(type == "Geometry")
	//		theMonicelliGeometryConverter_.loadGeometry(fileName);
}

//==============================================================================
bool VisualDataManager::getLiveDQMHistos(void) { return theLiveDQMHistos_; }

//==============================================================================
DQMHistosBase& VisualDataManager::getFileDQMHistos(void) { return theFileDQMHistos_; }

//==============================================================================
TFile* VisualDataManager::openFile(std::string fileName)
{
	auto tmpFile = fileMap_.find(fileName);
	if(tmpFile == fileMap_.end() || !tmpFile->second->IsOpen())
		fileMap_[fileName] = TFile::Open(fileName.c_str(), "RECREATE");

	if(fileMap_[fileName] == nullptr || !fileMap_[fileName]->IsOpen())
	{
		__GEN_SS__ << "Can't open file: " << fileName
		           << ". It is likely that the directory where the file should be stored does not exist or cannot be written." << __E__;
		__GEN_COUT_ERR__ << "\n" << ss.str();
		__GEN_SS_THROW__;
	}
	return fileMap_[fileName];
}

////==============================================================================
// const Visual3DEvents& VisualDataManager::getVisual3DEvents(void)
//{
//	return theMonicelliEventAnalyzer_.getEvents();
//}
//
////==============================================================================
// const Visual3DGeometry& VisualDataManager::getVisual3DGeometry(void)
//{
//	return theMonicelliGeometryConverter_.getGeometry();
//}
