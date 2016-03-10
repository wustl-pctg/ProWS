#ifndef _SYNCSTREAM_H
#define _SYNCSTREAM_H

#include <sstream>
#include <iostream>
#include <mutex>

namespace cilkrr
{
	class syncstream
	{
	private:
		static thread_local std::ostringstream m_s;
		std::ostream& m_printer;
		static std::mutex m_mut;

	public:
		syncstream(std::ostream& printer = std::cout) : m_printer(printer) { }
		~syncstream();

		template <typename T>
		syncstream& operator<< (const T& t);
		syncstream& operator<< (const char* t);
		//		syncstream& operator<< (void* const& t);

		typedef syncstream& (*syncstream_manipulator)(syncstream&);
		syncstream& operator<<(syncstream_manipulator manip);

		void flush(bool newline = false);
		void erase();
	}; // class syncstream
	
	syncstream& endl(syncstream& os);
	extern syncstream sout;
}

#endif // _SYNCSTREAM_H
