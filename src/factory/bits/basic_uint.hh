namespace factory {

	namespace bits {

		constexpr char
		const_tolower(char ch) noexcept {
			return ch >= 'A' && ch <= 'F' ? ('a'+ch-'A') : ch;
		}

		template<unsigned int radix, class Ch>
		constexpr Ch
		to_int(Ch ch) noexcept {
			return radix == 16 && ch >= 'a' ? ch-'a'+10 : ch-'0';
		}

		inline int
		get_stream_radix(const std::ios_base& out) noexcept {
			return out.flags() & std::ios_base::hex ? 16 :
				out.flags() & std::ios_base::oct ? 8 : 10;
		}

		template<class Uint, unsigned int radix, class It>
		constexpr Uint
		do_parse_uint(It first, It last, Uint val=0) noexcept {
			return first == last ? val
				: do_parse_uint<Uint, radix>(first+1, last, 
				val*radix + to_int<radix>(const_tolower(*first)));
		}

		template<class Uint, std::size_t n, class Arr>
		constexpr Uint
		parse_uint(Arr arr) noexcept {
			return
				n >= 2 && arr[0] == '0' && arr[1] == 'x' ? do_parse_uint<Uint,16>(arr+2, arr + n) :
				n >= 2 && arr[0] == '0' && arr[1] >= '1' && arr[1] <= '9' ? do_parse_uint<Uint,8>(arr+1, arr + n) :
				do_parse_uint<Uint,10>(arr, arr+n);
		}

		/*
		 * Copyright (c) 2008
		 * Evan Teran
		 *
		 * Permission to use, copy, modify, and distribute this software and its
		 * documentation for any purpose and without fee is hereby granted, provided
		 * that the above copyright notice appears in all copies and that both the
		 * copyright notice and this permission notice appear in supporting
		 * documentation, and that the same name not be used in advertising or
		 * publicity pertaining to distribution of the software without specific,
		 * written prior permission. We make no representations about the
		 * suitability this software for any purpose. It is provided "as is"
		 * without express or implied warranty.
		 */
		
		/// The code was modified to incorporate ``constexpr''
		/// and remove ``boost'' dependencies. The code was taken
		/// from http://www.codef00.com/code/uint128.h
		template<class T>
		struct basic_uint {
		
			typedef T base_type;

			struct xd_pair {
				basic_uint x;
				basic_uint d;
			};

			struct div_pair {
				basic_uint quotient;
				basic_uint remainder;
			};

			static_assert(
				std::is_unsigned<base_type>::value &&
				std::is_integral<base_type>::value,
				"bad base type");
		
			// constructors for all basic types
			constexpr
			basic_uint() noexcept:
				lo(0), hi(0) {}

			constexpr
			basic_uint(const basic_uint& rhs) noexcept:
				lo(rhs.lo), hi(rhs.hi) {}

			constexpr
			basic_uint(base_type value) noexcept:
				lo(value), hi(0) {}

			explicit constexpr
			basic_uint(base_type l, base_type h) noexcept:
				lo(l), hi(h) {}
		
			basic_uint&
			operator=(const basic_uint& other) noexcept {
				if (&other != this) {
					lo = other.lo;
					hi = other.hi;
				}
				return *this;
			}
		
			// comparison operators
			constexpr bool
			operator==(const basic_uint& o) const noexcept {
				return hi == o.hi && lo == o.lo;
			}
		
			constexpr bool
			operator<(const basic_uint& o) const noexcept {
				return (hi == o.hi) ? lo < o.lo : hi < o.hi;
			}
		
			// derived comparison operators
			constexpr bool
			operator!=(const basic_uint& y) const noexcept {
				return !operator==(y);
			}

			constexpr bool
			operator>(const basic_uint& y) const noexcept {
				return y < *this;
			}

			constexpr bool
			operator<=(const basic_uint& y) const noexcept {
				return !(y < *this);
			}

			constexpr bool
			operator>=(const basic_uint& y) const noexcept {
				return !operator<(y);
			}
		
			// unary operators
		    constexpr explicit
			operator bool() const noexcept {
				return hi != 0 || lo != 0;
			}
		
		    constexpr bool
			operator!() const noexcept {
				return !operator bool();
			}
		
			constexpr basic_uint
			operator-() const noexcept {
				// standard 2's compliment negation
				return ~(*this) + basic_uint(1);
			}
		
		    constexpr basic_uint
			operator~() const noexcept {
				return basic_uint(~lo, ~hi);
			}
		
			inline basic_uint&
			operator++() noexcept {
		    	if (++lo == 0) {
					++hi;
				}
		    	return *this;
			}
		
			inline basic_uint&
			operator--() noexcept {
		    	if (lo-- == 0) { --hi; }
		    	return *this;
			}
		
			// derived increment/decrement operators
			inline basic_uint
			operator++(int) const noexcept {
				return ++basic_uint(*this);
			}
		
			inline basic_uint
			operator--(int) const noexcept {
				return --basic_uint(*this);
			}
		
			// basic math operators
		    inline basic_uint&
			operator+=(const basic_uint& rhs) noexcept {
				const base_type old_lo = lo;
		
		    	lo += rhs.lo;
		    	hi += rhs.hi;
		
				if (lo < old_lo) {
					++hi;
				}
		
		    	return *this;
			}
		
		    inline basic_uint&
			operator-=(const basic_uint& b) noexcept {
				// it happens to be way easier to write it
				// this way instead of make a subtraction algorithm
				return *this += -b;
		    }
		
		    basic_uint&
			operator*=(const basic_uint& b) noexcept {
		
				// check for multiply by 0
				// result is always 0 :-P
				if (b == 0u) {
					lo = 0;
					hi = 0;
				// check we aren't multiplying by 1
				} else if (b != 1u) {
		
		    		basic_uint a(*this);
		    		basic_uint t = b;
		
		    		lo = 0;
		    		hi = 0;
		
		    		for (unsigned int i=0; i<NBITS; ++i) {
		        		if (t & 1) {
		            		*this += (a << i);
						}
		        		t >>= 1;
		    		}
				}
		
		    	return *this;
			}
		
		    inline basic_uint&
			operator|=(const basic_uint& b) noexcept {
		    	hi |= b.hi;
		    	lo |= b.lo;
		    	return *this;
			}
		
		    inline basic_uint&
			operator&=(const basic_uint& b) noexcept {
		    	hi &= b.hi;
		    	lo &= b.lo;
		    	return *this;
			}
		
			inline basic_uint&
			operator^=(const basic_uint& b) noexcept {
		    	hi ^= b.hi;
		    	lo ^= b.lo;
		    	return *this;
			}
		
			inline basic_uint&
			operator/=(const basic_uint& b) {
				basic_uint remainder;
				divide(*this, b, *this, remainder);
				return *this;
		    }
		
			inline basic_uint&
			operator%=(const basic_uint& b) {
				basic_uint quotient;
				divide(*this, b, quotient, *this);
				return *this;
		    }
		
		    inline basic_uint&
			operator<<=(const basic_uint& rhs) noexcept {
		
				unsigned int n = rhs.to_integer();
		
				if(n >= NBITS) {
					hi = 0;
					lo = 0;
				} else {
					const unsigned int halfsize = NBITS / 2;
		
		    		if(n >= halfsize){
		        		n -= halfsize;
		        		hi = lo;
		        		lo = 0;
		    		}
		
		    		if(n != 0) {
						// shift high half
		        		hi <<= n;
		
						const base_type mask(~(base_type(-1) >> n));
		
						// and add them to high half
		        		hi |= (lo & mask) >> (halfsize - n);
		
						// and finally shift also low half
		        		lo <<= n;
		    		}
				}
		
		    	return *this;
			}
		
			inline basic_uint&
			operator>>=(const basic_uint& rhs) noexcept {
		
				unsigned int n = rhs.to_integer();
		
				if(n >= NBITS) {
					hi = 0;
					lo = 0;
				} else {
					const unsigned int halfsize = NBITS / 2;
		
		    		if(n >= halfsize) {
		        		n -= halfsize;
		        		lo = hi;
		        		hi = 0;
		    		}
		
		    		if(n != 0) {
						// shift low half
		        		lo >>= n;
		
						// get lower N bits of high half
						const base_type mask(~(base_type(-1) << n));
		
						// and add them to low qword
		        		lo |= (hi & mask) << (halfsize - n);
		
						// and finally shift also high half
		        		hi >>= n;
		    		}
				}
		
		    	return *this;
			}
		
			// constexpr arithmetic operators
			constexpr basic_uint
			operator+(const basic_uint& rhs) const noexcept {
		    	return do_plus(*this, rhs, this->lo + rhs.lo);
			}
		
			constexpr basic_uint
			operator-(const basic_uint& rhs) const noexcept {
				return *this + (-rhs);
			}
		
			constexpr basic_uint
			operator*(const basic_uint& rhs) const noexcept {
				return rhs == NOUGHT ? basic_uint() :
					rhs == UNITY ? *this :
					do_mult(basic_uint(), basic_uint(*this), rhs, 0);
			}

			constexpr basic_uint
			operator/(const basic_uint& rhs) const noexcept {
				return do_divide(*this,
					do_divide_shift({basic_uint(1), rhs}, *this),
					0).quotient;
			}

			constexpr basic_uint
			operator%(const basic_uint& rhs) const noexcept {
				return do_divide(*this,
					do_divide_shift({basic_uint(1), rhs}, *this),
					0).remainder;
			}
		
			// derived bit-wise operators
			constexpr basic_uint
			operator&(const basic_uint& rhs) const noexcept {
				return basic_uint(lo & rhs.lo, hi & rhs.hi);
			}
		
			constexpr basic_uint
			operator|(const basic_uint& rhs) const noexcept {
				return basic_uint(lo | rhs.lo, hi | rhs.hi);
			}
		
			constexpr basic_uint
			operator^(const basic_uint& rhs) const noexcept {
				return basic_uint(lo ^ rhs.lo, hi ^ rhs.hi);
			}
		
			constexpr basic_uint
			operator<<(const basic_uint& n) const noexcept {
				return
					n.lo >= NBITS ? basic_uint() :
					n.lo >= NBITS/2
					? do_left_shift(basic_uint(NOUGHT, lo), n.lo - NBITS/2, NBITS/2)
					: do_left_shift(*this, n.lo, NBITS/2);
			}
		
			constexpr basic_uint
			operator>>(const basic_uint& n) const noexcept {
				return
					n.lo >= NBITS ? basic_uint() :
					n.lo >= NBITS/2
					? do_right_shift(basic_uint(hi, NOUGHT), n.lo - NBITS/2, NBITS/2)
					: do_right_shift(*this, n.lo, NBITS/2);
			}
		
			friend std::ostream&
			operator<<(std::ostream& out, const basic_uint& n) {
				std::ostream::sentry s(out);
				if (s) {
					int radix = bits::get_stream_radix(out);
			    	if (n == 0) { return out << '0'; }
					// at worst it will be NBITS digits (base 2) so make our buffer
					// that plus room for null terminator
			    	char buf[NBITS + 1] = {};
		//			buf[sizeof(buf) - 1] = '\0';
			
			    	basic_uint ii(n);
			    	int i = NBITS - 1;
			
			    	while (ii != 0 && i) {
						basic_uint remainder;
						divide(ii, basic_uint(radix), ii, remainder);
			        	buf[--i] = "0123456789abcdefghijklmnopqrstuvwxyz"[remainder.to_integer()];
			    	}
			
			    	out << buf + i;
				}
				return out;
			}
		
			friend std::istream&
			operator>>(std::istream& in, basic_uint& rhs) {
				std::istream::sentry s(in);
				if (s) {
					rhs = 0;
					unsigned int radix = bits::get_stream_radix(in);
					char ch;
					while (in >> ch) {
						unsigned int n = radix;
						switch (radix) {
							case 16: {
								ch = std::tolower(ch);
								if (ch >= 'a' && ch <= 'f') {
									n = ch - 'a' + 10;
								} else if (ch >= '0' && ch <= '9') {
									n = ch - '0';
								}
							} break;
							case 8:	
								if (ch >= '0' && ch <= '7') {
									n = ch - '0';
								}
								break;
							case 10:
							default:
								if (ch >= '0' && ch <= '9') {
									n = ch - '0';
								}
								break;
						}
						if (n == radix) {
							break;
						}
		
						rhs *= radix;
						rhs += n;
					}
				}
				return in;
			}
		
		private:
		
			constexpr int
			to_integer() const noexcept {
				return static_cast<int>(lo);
			}
		
			static constexpr
			basic_uint do_plus(const basic_uint& lhs, const basic_uint& rhs, const base_type new_lo) noexcept {
				return basic_uint(new_lo, lhs.hi + rhs.hi
					+ (new_lo < lhs.lo ? base_type(1) : base_type(0)));
			}

			static constexpr
			basic_uint do_plus(const basic_uint& lhs, const base_type& rhs, const base_type new_lo) noexcept {
				return basic_uint(new_lo, lhs.hi + (new_lo < lhs.lo ? base_type(1) : base_type(0)));
			}
		
			static constexpr basic_uint
			do_left_shift(const basic_uint& lhs, base_type n, unsigned int halfsize) noexcept {
				return n == 0 ? lhs : basic_uint(lhs.lo << n,
					(lhs.hi << n) | ((lhs.lo & (~(base_type(-1) >> n))) >> (halfsize - n)));
			}
		
			static constexpr basic_uint
			do_right_shift(const basic_uint& lhs, base_type n, unsigned int halfsize) noexcept {
				return n == 0 ? lhs : basic_uint(
					(lhs.lo >> n) | ((lhs.hi & (~(base_type(-1) << n))) << (halfsize - n)),
					lhs.hi >> n);
			}

			static constexpr basic_uint
			do_mult(basic_uint lhs, const basic_uint& a, basic_uint t, unsigned int i) noexcept {
				return i == NBITS ? lhs :
					(t & 1)
					? do_mult(lhs + (a << i), a, t >> 1, i+1)
					: do_mult(lhs, a, t >> 1, i+1);
			}

			static constexpr xd_pair
			do_divide_shift(xd_pair pair, const basic_uint& n) {
				return ((n >= pair.d) && (((pair.d >> (NBITS - 1)) & 1) == 0))
					? do_divide_shift({pair.x << 1, pair.d << 1}, n)
					: pair;
			}

			static constexpr div_pair
			do_divide(basic_uint n, xd_pair pair, basic_uint answer=0) {
				return pair.x == 0
					? div_pair{answer, n}
					: (
						n >= pair.d
							? do_divide(n - pair.d, {pair.x >> 1, pair.d >> 1}, answer | pair.x)
							: do_divide(n, {pair.x >> 1, pair.d >> 1}, answer)
					);
			}
		
			static void
			divide(const basic_uint& numerator, const basic_uint& denominator,
				basic_uint& quotient, basic_uint& remainder)
			{
				if (denominator == 0) {
					throw std::domain_error("divide by zero");
				} else {
					basic_uint n      = numerator;
					basic_uint d      = denominator;
					basic_uint x      = 1;
					basic_uint answer = 0;
			
					while ((n >= d) && (((d >> (NBITS - 1)) & 1) == 0)) {
						x <<= 1;
						d <<= 1;
					}
			
					while (x != 0) {
						if(n >= d) {
							n -= d;
							answer |= x;
						}
			
						x >>= 1;
						d >>= 1;
					}
			
					quotient = answer;
					remainder = n;
				}
			}
		
			base_type lo = 0;
			base_type hi = 0;
		
			static const unsigned int NBITS =
				(sizeof(base_type) + sizeof(base_type))
				* std::numeric_limits<unsigned char>::digits;
			
			static constexpr const base_type NOUGHT = 0;
			static constexpr const base_type UNITY = 1;
		};

	}

}