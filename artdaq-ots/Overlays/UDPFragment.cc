#include "artdaq-ots/Overlays/UDPFragment.hh"

std::ostream& ots::operator<<(std::ostream& os, UDPFragment const& f)
{
	os << "UDPFragment_event_size: " << f.hdr_event_size() << ", data_type: " << f.hdr_data_type() << "\n";

	return os;
}
