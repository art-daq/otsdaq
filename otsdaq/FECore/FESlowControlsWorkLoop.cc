#include "otsdaq/FECore/FESlowControlsWorkLoop.h"
#include "otsdaq/Macros/CoutMacros.h"

#include "otsdaq/FECore/FEVInterface.h"

#include <iostream>
#include <sstream>

using namespace ots;

bool FESlowControlsWorkLoop::workLoopThread(toolbox::task::WorkLoop* /*workLoop*/) { return interface_->slowControlsRunning(); }
