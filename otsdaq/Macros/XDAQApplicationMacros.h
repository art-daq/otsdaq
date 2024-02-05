#ifndef OTSDAQ_CORE_MACROS_XDAQAPPLICATIONMACROS_H
#define OTSDAQ_CORE_MACROS_XDAQAPPLICATIONMACROS_H

#undef XDAQ_INSTANTIATOR
#undef XDAQ_INSTANTIATOR_IMPL

/*! The XDAQ_INSTANTIATOR macro needs to put this macro into the header file (.h) class
 * declaration */
#define XDAQ_INSTANTIATOR() static xdaq::Application* instantiate(xdaq::ApplicationStub* s)

/*! the XDAQ_INSTANTIATOR_IMPL(ns1::ns2::...) macro needs to be put into the
 * implementation file (.cc) of the XDAQ application */
#define XDAQ_INSTANTIATOR_IMPL(QUALIFIED_CLASS_NAME)                                  \
	xdaq::Application* QUALIFIED_CLASS_NAME::instantiate(xdaq::ApplicationStub* stub) \
	{                                                                                 \
		return new QUALIFIED_CLASS_NAME(stub);                                        \
	}

#endif
