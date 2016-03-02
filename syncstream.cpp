#include "syncstream.h"

namespace cilkrr
{
	syncstream::~syncstream()
	{
		if (m_s.str() != "") {
			std::string warning = "Warning: leftover data in syncstream object:\n";
			std::string output = warning + m_s.str();
			erase();
			m_s << output;
			flush(true);
		}
	}

	template <typename T>
	syncstream& syncstream::operator<< (const T& t)
	{
		m_s << t;
		return *this;
	}

	syncstream& syncstream::operator<< (const char* t)
	{
		m_s << t;
		return *this;
	}

	syncstream& syncstream::operator<< (syncstream_manipulator manip)
	{
		return manip(*this);
	}

	syncstream& endl(syncstream& os)
	{
		os.flush(true);
		return os;
	}

	void syncstream::flush(bool newline)
	{
		{ std::lock_guard<std::mutex> lock(m_mut);
			m_printer << m_s.str();
			if (newline) m_printer << std::endl;
			m_printer.flush();
		}
		erase();
	}

	void syncstream::erase()
	{
		m_s.str("");
		m_s.clear();
	}

	template syncstream& syncstream::operator<< <std::string> (const std::string&);
	template syncstream& syncstream::operator<< <int> (const int&);
	syncstream sout(std::cout);
} // namespace cilkrr
