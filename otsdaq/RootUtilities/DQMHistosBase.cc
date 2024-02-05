#include "otsdaq/RootUtilities/DQMHistosBase.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/RootUtilities/VisualDataManager.h"

#include <TDirectory.h>
#include <TFile.h>
#include <TStyle.h>

#include <ctime>
#include <iostream>

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "DQMHistos"
#define mfSubject_ (std::string("DQMHistos"))

//==============================================================================
DQMHistosBase::DQMHistosBase(void) { gStyle->SetPalette(1); }

//==============================================================================
DQMHistosBase::~DQMHistosBase(void) { closeFile(); }

//==============================================================================
bool DQMHistosBase::isFileOpen(void)
{
	if(theFile_ == nullptr || !theFile_->IsOpen())
		return false;
	return true;
}

//==============================================================================
void DQMHistosBase::openFile(std::string fileName)
{
	closeFile();
	myDirectory_ = nullptr;
	theFile_     = theDataManager_->openFile(fileName);

	theFile_->cd();
	myDirectory_ = theFile_;
}

//==============================================================================
void DQMHistosBase::save(void)
{
	if(theFile_ != nullptr)
	{
		if(autoSave_)
			theFile_->Write("", TObject::kOverwrite);  // write the histogram to the file with kOverwrite update option
		else
			theFile_->Write();  // Lorenzo changed 2023-04-07 to kOverwrite
	}
}

//==============================================================================
void DQMHistosBase::autoSave(bool force)  // The file will be saved if currentTime - beginTimeTime_ is >= autoSaveInterval_
{
	if(!autoSave_)
		return;

	time_t currentTime;
	time(&currentTime);
	if(beginTime_ == 0)
	{
		theFile_->Write("", TObject::kOverwrite);  // write the histogram to the file with kOverwrite update option
		beginTime_ = currentTime;
		return;
	}

	if(force || currentTime - beginTime_ >= autoSaveInterval_)
	{
		theFile_->Write("", TObject::kOverwrite);  // write the histogram to the file with kOverwrite update option
		beginTime_ = currentTime;
	}
}

//==============================================================================
void DQMHistosBase::closeFile(void)
{
	if(theFile_ != nullptr)
	{
		if(theFile_->IsOpen())
			theFile_->Close();
		theFile_ = nullptr;
	}
}

//==============================================================================
TObject* DQMHistosBase::get(std::string name)
{
	if(theFile_ != nullptr)
		return theFile_->Get(name.c_str());
	return 0;
}
