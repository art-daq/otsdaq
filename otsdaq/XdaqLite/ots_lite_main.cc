// OTS_LITE replacement for xdaq.exe.
//
//   ots_lite_main -p <port> -h <host> [-e <profile.xml>] -c <partition.xml>
//
// Same command line the `ots` launcher uses for xdaq.exe, so `ots` works unchanged when
// xdaq.exe is a symlink to this program. The -e profile describes xdaq internals
// (executive, peer transports, hyperdaq) and is ignored; this program IS that executive.
//
// The -c partition (already env-expanded by `ots`) lists, for this context:
//     <xc:Context id="0" url="http://host:port">
//       <xc:Application class="ots::X" id="N" instance="1" .../>
//       <xc:Module>/path/libX.so</xc:Module>
//
// Each module is dlopen'ed (its XDAQ_INSTANTIATOR_IMPL registrar then self-registers the
// class), each application is instantiated with an xdaq::ApplicationStub, and a small
// multi-threaded HTTP/1.1 server routes
//     /WebPath/<file>                              -> static file under $OTSDAQ_WEB_PATH
//     /urn:xdaq-application:lid=<N>/<Name>[?...]  -> the xgi handler <Name> bound by app N
//     /urn:xdaq-application:lid=<N>[/]            -> that app's "Default" handler
//     POST with text/xml to lid=<N>                -> SOAP dispatch to app N
// No multipart upload yet (handlers that use getFiles() see none).

#include "cgicc/Cgicc.h"
#include "xdaq/Application.h"
#include "xdaq/ApplicationContext.h"
#include "xgi/Input.h"
#include "xgi/Output.h"

#include <xercesc/dom/DOM.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>

#include <arpa/inet.h>
#include <dlfcn.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
// ---------------------------------------------------------------------------
// partition XML
// ---------------------------------------------------------------------------
struct AppSpec
{
	std::string  className;
	unsigned int lid      = 0;
	unsigned int instance = 1;
	std::string  module;
};

std::string xs(const XMLCh* s)
{
	char*       c = xercesc::XMLString::transcode(s);
	std::string r = c ? c : "";
	xercesc::XMLString::release(&c);
	return r;
}

std::string localName(xercesc::DOMElement* e)
{
	std::string n = xs(e->getTagName());
	size_t      p = n.find(':');
	return p == std::string::npos ? n : n.substr(p + 1);
}

// Returns the context URL and the applications; modules are paired to the preceding
// <xc:Application> as xdaq does (each Application is followed by its Module).
std::string parsePartition(const std::string& file, std::vector<AppSpec>& apps)
{
	xercesc::XercesDOMParser parser;
	parser.setValidationScheme(xercesc::XercesDOMParser::Val_Never);
	parser.setDoNamespaces(false);
	parser.parse(file.c_str());
	xercesc::DOMDocument* doc = parser.getDocument();
	if(!doc || !doc->getDocumentElement())
		throw std::runtime_error("cannot parse partition XML " + file);

	std::string url;
	for(xercesc::DOMNode* c = doc->getDocumentElement()->getFirstChild(); c; c = c->getNextSibling())
	{
		if(c->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
			continue;
		auto* ctx = static_cast<xercesc::DOMElement*>(c);
		if(localName(ctx) != "Context")
			continue;
		url = xs(ctx->getAttribute(xercesc::XMLString::transcode("url")));
		for(xercesc::DOMNode* a = ctx->getFirstChild(); a; a = a->getNextSibling())
		{
			if(a->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
				continue;
			auto*       el = static_cast<xercesc::DOMElement*>(a);
			std::string ln = localName(el);
			if(ln == "Application")
			{
				AppSpec s;
				s.className = xs(el->getAttribute(xercesc::XMLString::transcode("class")));
				s.lid       = std::strtoul(xs(el->getAttribute(xercesc::XMLString::transcode("id"))).c_str(), nullptr, 10);
				std::string inst = xs(el->getAttribute(xercesc::XMLString::transcode("instance")));
				s.instance       = inst.empty() ? 1 : std::strtoul(inst.c_str(), nullptr, 10);
				apps.push_back(s);
			}
			else if(ln == "Module" && !apps.empty() && apps.back().module.empty())
			{
				apps.back().module = xs(el->getTextContent());
			}
		}
		break;  // one context per process
	}
	return url;
}

// ---------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------
struct Request
{
	std::string                        method, target, path, query, version, body;
	std::map<std::string, std::string> headers;  // lower-case keys
};

bool readRequest(int fd, Request& r)
{
	std::string buf;
	char        tmp[8192];
	size_t      hdrEnd = std::string::npos;
	while(hdrEnd == std::string::npos)
	{
		ssize_t n = ::read(fd, tmp, sizeof(tmp));
		if(n <= 0)
			return false;
		buf.append(tmp, n);
		hdrEnd = buf.find("\r\n\r\n");
		if(buf.size() > (1u << 20))
			return false;
	}
	std::istringstream head(buf.substr(0, hdrEnd));
	std::string        line;
	std::getline(head, line);
	if(!line.empty() && line.back() == '\r')
		line.pop_back();
	{
		std::istringstream rl(line);
		rl >> r.method >> r.target >> r.version;
	}
	while(std::getline(head, line))
	{
		if(!line.empty() && line.back() == '\r')
			line.pop_back();
		size_t c = line.find(':');
		if(c == std::string::npos)
			continue;
		std::string k = line.substr(0, c), v = line.substr(c + 1);
		for(auto& ch : k)
			ch = std::tolower(ch);
		size_t s = v.find_first_not_of(' ');
		r.headers[k] = s == std::string::npos ? "" : v.substr(s);
	}
	size_t clen = 0;
	if(r.headers.count("content-length"))
		clen = std::strtoul(r.headers["content-length"].c_str(), nullptr, 10);
	r.body = buf.substr(hdrEnd + 4);
	while(r.body.size() < clen)
	{
		ssize_t n = ::read(fd, tmp, sizeof(tmp));
		if(n <= 0)
			return false;
		r.body.append(tmp, n);
	}
	r.body.resize(clen);
	size_t q = r.target.find('?');
	r.path   = q == std::string::npos ? r.target : r.target.substr(0, q);
	r.query  = q == std::string::npos ? "" : r.target.substr(q + 1);
	return true;
}

void writeAll(int fd, const std::string& s)
{
	size_t off = 0;
	while(off < s.size())
	{
		ssize_t n = ::write(fd, s.data() + off, s.size() - off);
		if(n <= 0)
			return;
		off += n;
	}
}

void sendResponse(int fd, int status, const std::string& reason, const std::vector<std::pair<std::string, std::string>>& headers, const std::string& body)
{
	std::ostringstream o;
	o << "HTTP/1.1 " << status << " " << reason << "\r\n";
	bool hasType = false;
	for(const auto& h : headers)
	{
		if(strcasecmp(h.first.c_str(), "content-type") == 0)
			hasType = true;
		o << h.first << ": " << h.second << "\r\n";
	}
	if(!hasType)
		o << "Content-Type: text/html; charset=utf-8\r\n";
	o << "Content-Length: " << body.size() << "\r\nConnection: close\r\n\r\n";
	writeAll(fd, o.str() + body);
}

std::string mimeFor(const std::string& path)
{
	size_t      d   = path.rfind('.');
	std::string ext = d == std::string::npos ? "" : path.substr(d + 1);
	for(auto& c : ext)
		c = std::tolower(c);
	static const std::map<std::string, std::string> m = {
	    {"html", "text/html; charset=utf-8"}, {"htm", "text/html; charset=utf-8"}, {"js", "application/javascript"}, {"css", "text/css"}, {"png", "image/png"}, {"jpg", "image/jpeg"}, {"jpeg", "image/jpeg"}, {"gif", "image/gif"}, {"svg", "image/svg+xml"}, {"ico", "image/x-icon"}, {"json", "application/json"}, {"xml", "text/xml"}, {"txt", "text/plain"}, {"woff", "font/woff"}, {"woff2", "font/woff2"}, {"ttf", "font/ttf"}, {"mp3", "audio/mpeg"}, {"wav", "audio/wav"}};
	auto it = m.find(ext);
	return it == m.end() ? "application/octet-stream" : it->second;
}

std::string urlDecodePath(const std::string& s) { return cgicc::form_urldecode(s); }

// ---------------------------------------------------------------------------
// the context
// ---------------------------------------------------------------------------
struct LiteContext
{
	std::unique_ptr<xdaq::ApplicationContext>          context;
	std::map<unsigned int, xdaq::Application*>         apps;  // lid -> app
	std::vector<std::unique_ptr<xdaq::ApplicationStub>> stubs;
	std::string                                        webRoot;
	std::string                                        host;
	unsigned short                                     port = 0;
};

void serveStatic(int fd, LiteContext& ctx, const std::string& path)
{
	std::string rel = urlDecodePath(path.substr(std::string("/WebPath").size()));
	if(rel.find("..") != std::string::npos)
	{
		sendResponse(fd, 403, "Forbidden", {}, "Forbidden");
		return;
	}
	std::string   file = ctx.webRoot + rel;
	std::ifstream in(file, std::ios::binary);
	if(!in)
	{
		sendResponse(fd, 404, "Not Found", {}, "Not found: " + path);
		return;
	}
	std::ostringstream body;
	body << in.rdbuf();
	sendResponse(fd, 200, "OK", {{"Content-Type", mimeFor(file)}, {"Cache-Control", "no-cache"}}, body.str());
}

void serveApp(int fd, LiteContext& ctx, const Request& req, const std::string& remoteAddr)
{
	// /urn:xdaq-application:lid=<N>[/<Name>]
	static const std::string prefix = "/urn:xdaq-application:lid=";
	size_t                   p      = prefix.size();
	size_t                   slash  = req.path.find('/', p);
	unsigned int             lid    = std::strtoul(req.path.substr(p, slash == std::string::npos ? std::string::npos : slash - p).c_str(), nullptr, 10);
	std::string              name   = slash == std::string::npos ? "" : req.path.substr(slash + 1);
	if(name.empty())
		name = "Default";

	auto it = ctx.apps.find(lid);
	if(it == ctx.apps.end())
	{
		sendResponse(fd, 404, "Not Found", {}, "No application with lid=" + std::to_string(lid));
		return;
	}
	xdaq::Application* app = it->second;

	auto ctIt = req.headers.find("content-type");
	if(req.method == "POST" && ctIt != req.headers.end() && ctIt->second.find("text/xml") != std::string::npos)
	{
		// SOAP: body element local name = command, attributes = parameters
		xoap::MessageReference msg = xoap::createMessage();
		try
		{
			msg->readFrom(req.body);
			xoap::MessageReference reply = app->dispatchSOAP(msg);
			std::string            out;
			reply->writeTo(out);
			sendResponse(fd, 200, "OK", {{"Content-Type", "text/xml"}}, out);
		}
		catch(const std::exception& e)
		{
			sendResponse(fd, 500, "SOAP Error", {{"Content-Type", "text/plain"}}, e.what());
		}
		return;
	}

	cgicc::CgiEnvironment env;
	env.requestMethod_ = req.method;
	env.queryString_   = req.query;
	env.postData_      = req.body;
	env.remoteAddr_    = remoteAddr;
	env.remoteHost_    = remoteAddr;
	env.serverName_    = ctx.host;
	env.serverPort_    = ctx.port;
	env.scriptName_    = prefix + std::to_string(lid);
	env.pathInfo_      = "/" + name;
	if(ctIt != req.headers.end())
		env.contentType_ = ctIt->second;
	auto ua = req.headers.find("user-agent");
	if(ua != req.headers.end())
		env.userAgent_ = ua->second;
	auto ref = req.headers.find("referer");
	if(ref != req.headers.end())
		env.referrer_ = ref->second;
	auto ck = req.headers.find("cookie");
	if(ck != req.headers.end())
		env.cookies_ = ck->second;

	xgi::Input  in(env, req.query, req.body);
	xgi::Output out;
	try
	{
		if(!app->dispatchHTTP(name, &in, &out))
		{
			sendResponse(fd, 404, "Not Found", {}, "Application lid=" + std::to_string(lid) + " has no handler '" + name + "'");
			return;
		}
	}
	catch(const std::exception& e)
	{
		sendResponse(fd, 500, "Internal Server Error", {{"Content-Type", "text/plain"}}, std::string("Handler '") + name + "' threw: " + e.what());
		return;
	}
	catch(...)
	{
		sendResponse(fd, 500, "Internal Server Error", {{"Content-Type", "text/plain"}}, std::string("Handler '") + name + "' threw an unknown exception");
		return;
	}
	const auto& hdr     = out.getHTTPResponseHeader();
	auto        headers = hdr.getHeaders();
	for(const auto& c : hdr.getCookies())
		headers.emplace_back("Set-Cookie", c.getName() + "=" + c.getValue() + "; Path=/");
	sendResponse(fd, hdr.getStatusCode(), hdr.getReasonPhrase(), headers, out.str());
}

void handleConnection(int fd, LiteContext* ctx, std::string remoteAddr)
{
	Request req;
	if(readRequest(fd, req))
	{
		try
		{
			if(req.path.rfind("/WebPath/", 0) == 0)
				serveStatic(fd, *ctx, req.path);
			else if(req.path.rfind("/urn:xdaq-application:lid=", 0) == 0)
				serveApp(fd, *ctx, req, remoteAddr);
			else if(req.path == "/" || req.path.empty())
				sendResponse(fd, 200, "OK", {{"Content-Type", "text/plain"}}, "otsdaq lite context on " + ctx->host + ":" + std::to_string(ctx->port) + "\n");
			else
				sendResponse(fd, 404, "Not Found", {}, "Not found: " + req.path);
		}
		catch(const std::exception& e)
		{
			sendResponse(fd, 500, "Internal Server Error", {{"Content-Type", "text/plain"}}, e.what());
		}
	}
	::shutdown(fd, SHUT_RDWR);
	::close(fd);
}

std::atomic<bool> running{true};
void              onSignal(int) { running = false; }

void usage()
{
	std::cerr << "usage: ots_lite_main -p <port> -h <host> [-e <profile.xml>] -c <partition.xml>\n";
}
}  // namespace

int main(int argc, char** argv)
{
	std::string host = "localhost", profile, partition;
	int         port = 0;
	for(int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		auto        next = [&](std::string& dst) {
            if(i + 1 >= argc)
            {
                usage();
                std::exit(1);
            }
            dst = argv[++i];
		};
		if(a == "-p")
		{
			std::string s;
			next(s);
			port = std::atoi(s.c_str());
		}
		else if(a == "-h")
			next(host);
		else if(a == "-e")
			next(profile);
		else if(a == "-c")
			next(partition);
		else
		{
			usage();
			return 1;
		}
	}
	if(port <= 0 || partition.empty())
	{
		usage();
		return 1;
	}

	xercesc::XMLPlatformUtils::Initialize();

	LiteContext ctx;
	ctx.host = host;
	ctx.port = port;
	const char* web = getenv("OTSDAQ_WEB_PATH");
	ctx.webRoot      = web ? web : "";

	std::vector<AppSpec> specs;
	std::string          url;
	try
	{
		url = parsePartition(partition, specs);
	}
	catch(const std::exception& e)
	{
		std::cerr << "ots_lite_main: " << e.what() << "\n";
		return 1;
	}
	if(url.empty())
		url = "http://" + host + ":" + std::to_string(port);
	ctx.context.reset(new xdaq::ApplicationContext(url));
	std::cout << "ots_lite_main: context " << url << " (" << specs.size() << " applications), WebPath=" << ctx.webRoot << std::endl;

	// Load every module first, then instantiate. As real xdaq.exe does, each class is found
	// by the exported symbol ots::<Class>::instantiate(xdaq::ApplicationStub*) that
	// XDAQ_INSTANTIATOR_IMPL defines; its Itanium-mangled name is built from the class name.
	std::map<std::string, void*> handles;
	for(const auto& s : specs)
	{
		if(s.module.empty() || handles.count(s.module))
			continue;
		void* h = dlopen(s.module.c_str(), RTLD_NOW | RTLD_GLOBAL);
		if(!h)
		{
			std::cerr << "ots_lite_main: cannot load " << s.module << ": " << dlerror() << "\n";
			return 1;
		}
		handles[s.module] = h;
	}
	for(const auto& s : specs)
	{
		// "ots::WizardSupervisor" -> namespaces + class, mangled as _ZN3ots16WizardSupervisor11instantiateEPN4xdaq15ApplicationStubE
		std::string mangled = "_ZN";
		{
			size_t pos = 0;
			while(true)
			{
				size_t      sep  = s.className.find("::", pos);
				std::string part = s.className.substr(pos, sep == std::string::npos ? std::string::npos : sep - pos);
				mangled += std::to_string(part.size()) + part;
				if(sep == std::string::npos)
					break;
				pos = sep + 2;
			}
		}
		mangled += "11instantiateEPN4xdaq15ApplicationStubE";

		void* sym = nullptr;
		for(const auto& h : handles)
			if((sym = dlsym(h.second, mangled.c_str())))
				break;
		if(!sym)
		{
			std::cerr << "ots_lite_main: no " << s.className << "::instantiate(xdaq::ApplicationStub*) exported by " << s.module << " (symbol " << mangled << "; missing XDAQ_INSTANTIATOR_IMPL?)\n";
			return 1;
		}
		xdaq::ApplicationInstantiator make = reinterpret_cast<xdaq::ApplicationInstantiator>(sym);
		const xdaq::ApplicationDescriptor* d = ctx.context->addApplication(nullptr, s.className, s.lid, s.instance);
		ctx.stubs.emplace_back(new xdaq::ApplicationStub(ctx.context.get(), d));
		std::cout << "ots_lite_main: instantiating " << s.className << " lid=" << s.lid << std::endl;
		try
		{
			xdaq::Application* app = make(ctx.stubs.back().get());
			ctx.apps[s.lid]        = app;
			ctx.context->setApplication(s.lid, app);
		}
		catch(const std::exception& e)
		{
			std::cerr << "ots_lite_main: constructing " << s.className << " failed: " << e.what() << "\n";
			return 1;
		}
	}

	int srv = ::socket(AF_INET, SOCK_STREAM, 0);
	int one = 1;
	setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	sockaddr_in addr{};
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port        = htons(port);
	if(::bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::listen(srv, 64) != 0)
	{
		std::cerr << "ots_lite_main: cannot listen on port " << port << ": " << strerror(errno) << "\n";
		return 1;
	}
	signal(SIGINT, onSignal);
	signal(SIGTERM, onSignal);
	signal(SIGPIPE, SIG_IGN);
	std::cout << "ots_lite_main: listening on http://" << host << ":" << port << std::endl;

	while(running)
	{
		sockaddr_in cli{};
		socklen_t   len = sizeof(cli);
		int         fd  = ::accept(srv, reinterpret_cast<sockaddr*>(&cli), &len);
		if(fd < 0)
		{
			if(errno == EINTR)
				continue;
			break;
		}
		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
		char ip[INET_ADDRSTRLEN] = {0};
		inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
		std::thread(handleConnection, fd, &ctx, std::string(ip)).detach();
	}
	::close(srv);
	std::cout << "ots_lite_main: exiting" << std::endl;
	return 0;
}
