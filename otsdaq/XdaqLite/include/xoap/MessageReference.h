// OTS_LITE stand-in for xdaq's xoap SOAP message classes.
// otsdaq's SOAP protocol (see SOAPUtilities::translate) is: exactly one body element whose
// local name is the command and whose attributes are the parameters. The message is modeled
// as that, with just enough DOM-like surface for SOAPUtilities to compile unchanged.
//
// As in real xoap, SOAPElement is a HANDLE: copies share the underlying element, so
// `body.addBodyElement(name).addAttribute(...)` and edits through getChildElements() copies
// modify the message.
#ifndef _xoap_MessageReference_h_
#define _xoap_MessageReference_h_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "xcept/Exception.h"

namespace xoap
{
namespace exception
{
class Exception : public xcept::Exception
{
  public:
	using xcept::Exception::Exception;
};
}  // namespace exception

typedef std::string XMLCh;
inline std::string  XMLCh2String(const XMLCh* s) { return s ? *s : std::string(); }

class SOAPName
{
  public:
	SOAPName() {}
	SOAPName(const std::string& local, const std::string& prefix = "", const std::string& uri = "")
	    : local_(local)
	    , prefix_(prefix)
	    , uri_(uri) {}
	std::string getLocalName() const { return local_; }
	std::string getPrefix() const { return prefix_; }
	std::string getURI() const { return uri_; }
	std::string getQualifiedName() const { return prefix_.empty() ? local_ : prefix_ + ":" + local_; }
	bool        operator==(const SOAPName& o) const { return local_ == o.local_; }
	bool        operator!=(const SOAPName& o) const { return !(*this == o); }

  private:
	std::string local_, prefix_, uri_;
};

// Minimal Xerces-DOM facade so `element.getDOM()->getAttributes()` loops compile.
// Kept in xoap::lite so these names never collide with xercesc's DOM types, which otsdaq
// also uses unqualified in the same translation units.
namespace lite
{
class DOMNode
{
  public:
	DOMNode(const std::string& name, const std::string& value)
	    : name_(name)
	    , value_(value) {}
	const XMLCh* getNodeName() const { return &name_; }
	const XMLCh* getNodeValue() const { return &value_; }

  private:
	std::string name_, value_;
};

class DOMNamedNodeMap
{
  public:
	DOMNamedNodeMap() {}
	explicit DOMNamedNodeMap(const std::map<std::string, std::string>& attrs)
	{
		for(const auto& a : attrs)
			nodes_.emplace_back(a.first, a.second);
	}
	unsigned int getLength() const { return nodes_.size(); }
	DOMNode*     item(unsigned int i) { return &nodes_[i]; }

  private:
	std::vector<DOMNode> nodes_;
};

class DOMElement
{
  public:
	explicit DOMElement(const std::map<std::string, std::string>* attrs)
	    : attrs_(attrs) {}
	// Rebuilt on each call so attributes added after getDOM() are visible.
	DOMNamedNodeMap* getAttributes()
	{
		map_ = DOMNamedNodeMap(*attrs_);
		return &map_;
	}

  private:
	const std::map<std::string, std::string>* attrs_;
	DOMNamedNodeMap                           map_;
};
}  // namespace lite

class SOAPElement
{
	struct Impl
	{
		SOAPName                           name;
		std::map<std::string, std::string> attributes;
		std::string                        text;
		std::vector<SOAPElement>           children;
		std::unique_ptr<lite::DOMElement>  dom;
	};

  public:
	SOAPElement()
	    : impl_(std::make_shared<Impl>()) {}
	explicit SOAPElement(const SOAPName& name)
	    : SOAPElement() { impl_->name = name; }

	SOAPName    getElementName() const { return impl_->name; }
	void        addAttribute(const SOAPName& name, const std::string& value) { impl_->attributes[name.getLocalName()] = value; }
	std::string getAttributeValue(const SOAPName& name) const
	{
		auto it = impl_->attributes.find(name.getLocalName());
		if(it == impl_->attributes.end())
			return "";
		return it->second;
	}
	const std::map<std::string, std::string>& getAttributes() const { return impl_->attributes; }
	std::string                               getTextContent() const { return impl_->text; }
	void                                      addTextNode(const std::string& t) { impl_->text += t; }
	std::vector<SOAPElement>                  getChildElements() const { return impl_->children; }
	SOAPElement                               addChildElement(const SOAPName& n)
	{
		impl_->children.emplace_back(n);
		return impl_->children.back();
	}
	lite::DOMElement* getDOM()
	{
		if(!impl_->dom)
			impl_->dom.reset(new lite::DOMElement(&impl_->attributes));
		return impl_->dom.get();
	}

  private:
	std::shared_ptr<Impl> impl_;
};

class SOAPBody
{
  public:
	explicit SOAPBody(std::vector<SOAPElement>* elements)
	    : elements_(elements) {}
	SOAPElement addBodyElement(const SOAPName& name)
	{
		elements_->emplace_back(name);
		return elements_->back();
	}
	std::vector<SOAPElement> getChildElements() const { return *elements_; }
	bool                     hasFault() const { return false; }

  private:
	std::vector<SOAPElement>* elements_;
};

class SOAPEnvelope
{
  public:
	explicit SOAPEnvelope(std::vector<SOAPElement>* elements)
	    : elements_(elements) {}
	SOAPName createName(const std::string& local, const std::string& prefix = "", const std::string& uri = "") const { return SOAPName(local, prefix, uri); }
	SOAPBody getBody() const { return SOAPBody(elements_); }

  private:
	std::vector<SOAPElement>* elements_;
};

class SOAPPart
{
  public:
	explicit SOAPPart(std::vector<SOAPElement>* elements)
	    : elements_(elements) {}
	SOAPEnvelope getEnvelope() const { return SOAPEnvelope(elements_); }

  private:
	std::vector<SOAPElement>* elements_;
};

class MimeHeaders
{
  public:
	void        setHeader(const std::string& k, const std::string& v) { headers_[k] = v; }
	std::string getHeader(const std::string& k) const
	{
		auto it = headers_.find(k);
		return it == headers_.end() ? "" : it->second;
	}

  private:
	std::map<std::string, std::string> headers_;
};

class AttachmentPart
{
  public:
	void        setContent(const std::string& c) { content_ = c; }
	void        setContentId(const std::string& id) { contentId_ = id; }
	void        addMimeHeader(const std::string& k, const std::string& v) { headers_.setHeader(k, v); }
	std::string getContent() const { return content_; }
	std::string getContentId() const { return contentId_; }

  private:
	std::string content_, contentId_;
	MimeHeaders headers_;
};

class SOAPMessage
{
  public:
	SOAPPart        getSOAPPart() { return SOAPPart(&bodyElements_); }
	MimeHeaders*    getMimeHeaders() { return &mimeHeaders_; }
	AttachmentPart* createAttachmentPart()
	{
		attachments_.emplace_back(new AttachmentPart());
		return attachments_.back().get();
	}
	void                         addAttachmentPart(AttachmentPart*) {}
	std::vector<AttachmentPart*> getAttachments()
	{
		std::vector<AttachmentPart*> v;
		for(auto& a : attachments_)
			v.push_back(a.get());
		return v;
	}
	// Serialization helpers used by the lite transport.
	std::vector<SOAPElement>&       bodyElements() { return bodyElements_; }
	const std::vector<SOAPElement>& bodyElements() const { return bodyElements_; }
	void                            writeTo(std::string& out) const;
	void                            readFrom(const std::string& in);

  private:
	std::vector<SOAPElement>                     bodyElements_;
	MimeHeaders                                  mimeHeaders_;
	std::vector<std::shared_ptr<AttachmentPart>> attachments_;
};

typedef std::shared_ptr<SOAPMessage> MessageReference;

inline MessageReference createMessage() { return MessageReference(new SOAPMessage()); }
}  // namespace xoap

// Real xoap exposes the Xerces DOM types unqualified; otsdaq's SOAP code uses this one that way.
// Only this name is exported (the SOAP files that use it never see xercesc's own).
using DOMNamedNodeMap = xoap::lite::DOMNamedNodeMap;

#endif
