#ifndef artdaq_ots_Overlays_UDPFragmentWriter_hh
#define artdaq_ots_Overlays_UDPFragmentWriter_hh

////////////////////////////////////////////////////////////////////////
// UDPFragmentWriter
//
// Class derived from UDPFragment which allows writes to the data (for
// simulation purposes). Note that for this reason it contains
// non-const members which hide the const members in its parent class,
// UDPFragment, including its reference to the artdaq::Fragment
// object, artdaq_Fragment_, as well as its functions pointing to the
// beginning and end of ADC values in the fragment, dataBegin() and
// dataEnd()
//
////////////////////////////////////////////////////////////////////////

#include "artdaq-core/Data/Fragment.hh"
#include "artdaq-ots/Overlays/UDPFragment.hh"

#include <iostream>

namespace ots
{
class UDPFragmentWriter;
}

class ots::UDPFragmentWriter : public ots::UDPFragment
{
  public:
	UDPFragmentWriter(artdaq::Fragment& f);

	// These functions form overload sets with const functions from
	// ots::UDPFragment

	uint8_t* dataBegin();
	uint8_t* dataEnd();

	// We'll need to hide the const version of header in UDPFragment in
	// order to be able to perform writes

	Header* header_()
	{
		assert(artdaq_Fragment_.dataSizeBytes() >= sizeof(Header));
		return reinterpret_cast<Header*>(artdaq_Fragment_.dataBeginBytes());
	}

	void set_hdr_type(Header::data_type_t dataType) { header_()->type = dataType & 0xF; }

	void resize(size_t nBytes);

  private:
	size_t calc_event_size_words_(size_t nBytes);

	static size_t bytes_to_words_(size_t nBytes);

	// Note that this non-const reference hides the const reference in the base class
	artdaq::Fragment& artdaq_Fragment_;
};

// The constructor will expect the artdaq::Fragment object it's been
// passed to contain the artdaq::Fragment header + the
// UDPFragment::Metadata object, otherwise it throws

ots::UDPFragmentWriter::UDPFragmentWriter(artdaq::Fragment& f)
    : UDPFragment(f), artdaq_Fragment_(f)
{
	if(!f.hasMetadata() || f.dataSizeBytes() > 0)
	{
		throw cet::exception(
		    "Error in UDPFragmentWriter: Raw artdaq::Fragment object does not appear to "
		    "consist of (and only of) its own header + the UDPFragment::Metadata object");
	}

	// Allocate space for the header
	artdaq_Fragment_.resizeBytes(sizeof(Header));
}

inline uint8_t* ots::UDPFragmentWriter::dataBegin()
{
	// Make sure there's data past the UDPFragment header
	assert(artdaq_Fragment_.dataSizeBytes() >=
	       sizeof(Header) + sizeof(artdaq::Fragment::value_type));
	return reinterpret_cast<uint8_t*>(header_() + 1);
}

inline uint8_t* ots::UDPFragmentWriter::dataEnd()
{
	return dataBegin() + udp_data_words();
}

inline void ots::UDPFragmentWriter::resize(size_t nBytes)
{
	artdaq_Fragment_.resizeBytes(sizeof(Header::data_t) * calc_event_size_words_(nBytes));
	header_()->event_size = calc_event_size_words_(nBytes);
}

inline size_t ots::UDPFragmentWriter::calc_event_size_words_(size_t nBytes)
{
	return bytes_to_words_(nBytes) + hdr_size_words();
}

inline size_t ots::UDPFragmentWriter::bytes_to_words_(size_t nBytes)
{
	auto mod(nBytes % bytes_per_word_());
	return (mod == 0) ? nBytes / bytes_per_word_() : nBytes / bytes_per_word_() + 1;
}

#endif /* artdaq_demo_Overlays_UDPFragmentWriter_hh */
