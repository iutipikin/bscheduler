#ifndef DISCOVERY_DISCOVERY_HH
#define DISCOVERY_DISCOVERY_HH

namespace factory {

	namespace components {

		template<class I>
		inline
		I log2(I x) {
			I n = 0;
			while (x >>= 1) n++;
			return n;
		}

		inline
		uint32_t log(uint32_t x, uint32_t p = 1) {
			uint32_t n = 0;
			while (x >>= p) n++;
			return n;
		}

		template<class I>
		struct Interval {

			typedef I int_type;
		
			constexpr
			Interval() noexcept:
			_start(0), _end(0) {}

			constexpr
			Interval(I a, I b) noexcept:
			_start(a), _end(b) {}
			
			constexpr
			Interval(const Interval& rhs) noexcept:
			_start(rhs._start), _end(rhs._end) {}
		
			constexpr
			I start() const noexcept {
				return _start;
			}
			
			constexpr
			I end() const noexcept {
				return _end;
			}
		
			constexpr bool
			overlaps(const Interval& rhs) const noexcept {
				return (_start < rhs._start && _end > rhs._start)
					|| (rhs._start < _start && rhs._end > _start);
			}
		
			inline Interval&
			operator+=(const Interval& rhs) noexcept {
				_start = std::min(_start, rhs._start);
				_end = std::max(_end, rhs._end);
				return *this;
			}
			
			constexpr bool
			operator<(const Interval& rhs) const noexcept {
				return _start < rhs._start;
			}
		
			friend std::ostream&
			operator<<(std::ostream& out, const Interval& rhs) {
				return out << rhs._start << ' ' << rhs._end;
			}
		
			friend std::istream&
			operator>>(std::istream& in, Interval& rhs) {
				return in >> rhs._start >> rhs._end;
			}
		
			constexpr bool
			empty() const noexcept {
				return !(_start < _end);
			}

			constexpr I
			count() const noexcept {
				return _end - _start;
			}
		
		private:
			I _start, _end;
		};
		
		typedef Interval<uint32_t> Address_range;
		
	}

}

#endif // DISCOVERY_DISCOVERY_HH