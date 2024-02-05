#include "otsdaq/FECore/FEProducerVInterface.h"
#include "otsdaq/DataManager/DataManager.h"
#include "otsdaq/DataManager/DataManagerSingleton.h"

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "FEProducer"
#define mfSubject_ (std::string("FEProducer-") + DataProcessor::processorUID_)

//==============================================================================
FEProducerVInterface::FEProducerVInterface(const std::string&       interfaceUID,
                                           const ConfigurationTree& theXDAQContextConfigTree,
                                           const std::string&       interfaceConfigurationPath)
    : FEVInterface(interfaceUID, theXDAQContextConfigTree, interfaceConfigurationPath)
    , DataProducerBase(theXDAQContextConfigTree.getBackNode(interfaceConfigurationPath, 4).getValueAsString(),
                       theXDAQContextConfigTree.getNode(interfaceConfigurationPath + "/" + "LinkToDataBufferTable", 4).getValueAsString(),
                       interfaceUID /*processorID*/,
                       100 /*bufferSize*/)
{
	// NOTE!! be careful to not decorate with __FE_COUT__ because in the constructor the
	// base class versions of function (e.g. getInterfaceType) are called because the
	// derived class has not been instantiate yet!
	__GEN_COUT__ << "'" << interfaceUID << "' Constructed." << __E__;

	__GEN_COUTV__(interfaceConfigurationPath);
	ConfigurationTree appNode = theXDAQContextConfigTree.getBackNode(interfaceConfigurationPath, 2);

	__GEN_COUTV__(appNode.getValueAsString());

}  // end constructor()

FEProducerVInterface::~FEProducerVInterface(void)
{
	__FE_COUT__ << "Destructor." << __E__;
	// Take out of DataManager vector!

	__GEN_COUT__ << "FEProducer '" << DataProcessor::processorUID_ << "' is unregistering from DataManager Supervisor Buffer '"
	             << DataProcessor::supervisorApplicationUID_ << ":" << DataProcessor::bufferUID_ << ".'" << std::endl;

	DataManager* dataManager = (DataManagerSingleton::getInstance(supervisorApplicationUID_));

	dataManager->unregisterFEProducer(bufferUID_, DataProcessor::processorUID_);

	{
		__GEN_SS__;
		dataManager->dumpStatus(&ss);
		std::cout << ss.str() << __E__;
	}

	__GEN_COUT__ << "FEProducer '" << DataProcessor::processorUID_ << "' unregistered." << __E__;

	__FE_COUT__ << "Destructed." << __E__;
}

//==============================================================================
// copyToNextBuffer
//	This function copies a data string into the next
//		available buffer.
//
//	Here is example code for filling a data string to write
//
//			unsigned long long value = 0xA5; //this is 8-bytes
//			std::string buffer;
//			buffer.resize(8); //NOTE: this is inexpensive according to
// Lorenzo/documentation  in C++11 (only increases size once and doesn't decrease size)
// memcpy((void
//*)&buffer /*dest*/,(void *)&value /*src*/, 8 /*numOfBytes*/);
//
//	Note: This is somewhat inefficient because it makes a copy of the data.
//		It would be more efficient to call
//			FEProducerVInterface::getNextBuffer()
//			... fill the retrieved data string
//			FEProducerVInterface::writeCurrentBuffer()
//
//	If you are using the same dataToWrite string over and over.. it might not be that
// inefficient to use this.
//
void FEProducerVInterface::copyToNextBuffer(const std::string& dataToWrite)
{
	__FE_COUT__ << "Write Data: " << BinaryStringMacros::binaryNumberToHexString(dataToWrite) << __E__;

	DataProducerBase::write<std::string, std::map<std::string, std::string> >(dataToWrite);
	//
	//	FEProducerVInterface::getNextBuffer();
	//
	//	__FE_COUT__ << "Have next buffer " << FEProducerVInterface::dataP_ << __E__;
	//	__FE_COUT__ << "Have next buffer size " << FEProducerVInterface::dataP_->size() <<
	//__E__;
	//
	//
	//	FEProducerVInterface::dataP_->resize(dataToWrite.size());
	//
	//	__FE_COUT__ << "Have next buffer size " << FEProducerVInterface::dataP_->size() <<
	//__E__;
	//
	//	//*FEProducerVInterface::dataP_ = dataToWrite; //copy
	//	memcpy((void *)&(*FEProducerVInterface::dataP_)[0] /*dest*/,
	//			(void *)&dataToWrite[0] /*src*/, 8 /*numOfBytes*/);
	//
	//	__FE_COUT__ << "Data copied " << FEProducerVInterface::dataP_->size() << __E__;
	//
	//	__FE_COUT__ << "Copied Data: " <<
	//			BinaryStringMacros::binaryNumberToHexString(*FEProducerVInterface::dataP_)
	//<<
	//__E__;
	//
	//
	//	FEProducerVInterface::writeCurrentBuffer();

}  // end copyToNextBuffer()

//==============================================================================
// getNextBuffer
//	This function retrieves the next buffer data string.

//	Note: This is more efficient than FEProducerVInterface::writeToBuffer
//		because it does NOT makes a copy of the data.
//
//		You need to now
//			... fill the retrieved data string
//			FEProducerVInterface::writeCurrentBuffer()
//
std::string* FEProducerVInterface::getNextBuffer(void)
{
	if(DataProducerBase::attachToEmptySubBuffer(FEProducerVInterface::dataP_, FEProducerVInterface::headerP_) < 0)
	{
		__GEN_SS__ << "There are no available buffers! Retrying...after waiting 10 milliseconds!" << std::endl;
		__GEN_SS_THROW__;
	}

	return FEProducerVInterface::dataP_;
}  // end getNextBuffer()

//==============================================================================
// writeCurrentBuffer
//	This function writes the current buffer data string to the buffer.
//
void FEProducerVInterface::writeCurrentBuffer(void)
{
	__FE_COUT__ << "Writing data of size " << FEProducerVInterface::dataP_->size() << __E__;

	DataProducerBase::setWrittenSubBuffer<std::string, std::map<std::string, std::string> >();

	__FE_COUT__ << "Data written." << __E__;

}  // end writeCurrentBuffer()
