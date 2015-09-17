#ifndef STDX_PACKETBUF_HH
#define STDX_PACKETBUF_HH

#include <queue>
#include <cassert>

#include <stdx/streambuf.hh>

namespace stdx {

	template<class Ch, class Tr=std::char_traits<Ch>>
	struct basic_packetbuf: public basic_streambuf<Ch,Tr> {

		using typename basic_streambuf<Ch,Tr>::pos_type;
		using typename basic_streambuf<Ch,Tr>::off_type;
		using typename basic_streambuf<Ch,Tr>::char_type;
		using basic_streambuf<Ch,Tr>::eback;
		using basic_streambuf<Ch,Tr>::egptr;
		using basic_streambuf<Ch,Tr>::setg;
		typedef std::streamsize size_type;

		virtual void
		begin_packet() {}

		virtual void
		end_packet() {}

		virtual bool
		read_packet() { return false; }

	protected:

		char_type*
		payload_begin() {
			return eback() + _payloadpos;
		}

		char_type*
		payload_end() {
			return payload_begin() + payloadsize();
		}

		pos_type
		payloadpos() {
			return _payloadpos;
		}

		size_type
		payloadsize() {
			return _packetsize - (_payloadpos - _packetpos);
		}

		char_type*
		packet_begin() const {
			return eback() + _packetpos;
		}

		char_type*
		packet_end() const {
			return packet_begin() + _packetsize;
		}

		pos_type
		packetpos() {
			return _packetpos;
		}

		size_type
		packetsize() {
			return _packetsize;
		}

		void
		setpacket(pos_type pos1, pos_type pos2, size_type n) {
			_packetpos = pos1;
			_payloadpos = pos2;
			_packetsize = n;
			assert(_packetpos <= _payloadpos);
			assert(_payloadpos <= _packetpos + n);
		}

		void
		seekpayloadpos(off_type off) {
			assert(eback() <= payload_begin() + off);
			assert(payload_begin() + off <= egptr());
			setg(eback(), payload_begin() + off, egptr());
		}

		template<class Ch1, class Tr1>
		friend void
		append_payload(basic_packetbuf<Ch1,Tr1>& buf, basic_packetbuf<Ch1,Tr1>& rhs);

	private:

		pos_type _packetpos = 0;
		pos_type _payloadpos = 0;
		size_type _packetsize = 0;
	};

	template<class Ch, class Tr>
	void append_payload(basic_packetbuf<Ch,Tr>& buf, basic_packetbuf<Ch,Tr>& rhs) {
		const std::streamsize n = rhs.payloadsize();
		buf.sputn(rhs.payload_begin(), n);
		rhs.seekpayloadpos(n);
	}

	typedef basic_packetbuf<char> packetbuf;

}

#endif // STDX_PACKETBUF_HH
