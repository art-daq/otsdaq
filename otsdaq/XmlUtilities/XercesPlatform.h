#ifndef _ots_XercesPlatform_h_
#define _ots_XercesPlatform_h_

namespace ots
{
/// XercesPlatform
///	Thread-safe wrappers for xercesc::XMLPlatformUtils::Initialize() and Terminate().
///	Xerces keeps a plain (not thread-safe) reference count and must not have these called
///	from several threads at once. otsdaq creates an XmlDocument (which initializes and
///	terminates Xerces) per HTTP reply, and the web server answers each connection in its
///	own thread, so concurrent requests could corrupt the count, let it reach zero, and
///	free Xerces' global mutexes while another thread used them (SIGSEGV in
///	XMLMutex::lock, seen with a browser polling the Visualizer).
///	Both rethrow xercesc::XMLException like the originals.
struct XercesPlatform
{
	static void initialize(void);
	static void terminate(void);
};
}  // namespace ots

#endif
