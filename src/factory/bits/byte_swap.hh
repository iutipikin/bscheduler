namespace factory {

	namespace bits {

		/// @addtogroup byteswap Byte swap
		/// @brief Compile-time byte swapping functions.
		/// @{
		template<class T>
		inline T
		byte_swap (T n) noexcept {
			union {
				T x;
				unsigned char y[sizeof(T)];
			} tmp;
			tmp.x = n;
			std::reverse(tmp.y, tmp.y + sizeof(T));
			return tmp.x;
		}
		
		template<>
		constexpr uint8_t
		byte_swap<uint8_t>(uint8_t n) noexcept {
			return n;
		}
		
		template<>
		constexpr uint16_t
		byte_swap<uint16_t>(uint16_t n) noexcept {
			return ((n & 0xff00)>>8) | ((n & 0x00ff)<<8);
		}
		
		template<>
		constexpr uint32_t
		byte_swap<uint32_t>(uint32_t n) noexcept {
			#ifdef HAVE___BUILTIN_BSWAP32
			return __builtin_bswap32(n);
			#else
			return
				((n & UINT32_C(0xff000000)) >> 24) |
				((n & UINT32_C(0x00ff0000)) >> 8) |
				((n & UINT32_C(0x0000ff00)) << 8) |
				((n & UINT32_C(0x000000ff)) << 24);
			#endif
		}
		
		template<>
		constexpr uint64_t
		byte_swap<uint64_t>(uint64_t n) noexcept {
			#ifdef HAVE___BUILTIN_BSWAP64
			return __builtin_bswap64(n);
			#else
			return
				((n & UINT64_C(0xff00000000000000)) >> 56) |
				((n & UINT64_C(0x00ff000000000000)) >> 40) |
				((n & UINT64_C(0x0000ff0000000000)) >> 24) |
				((n & UINT64_C(0x000000ff00000000)) >> 8) |
				((n & UINT64_C(0x00000000ff000000)) << 8) |
				((n & UINT64_C(0x0000000000ff0000)) << 24) |
				((n & UINT64_C(0x000000000000ff00)) << 40) |
				((n & UINT64_C(0x00000000000000ff)) << 56);
			#endif
		}

		template<>
		inline uint128_t
		byte_swap(uint128_t x) noexcept {
			int i = sizeof(x) * std::numeric_limits<unsigned char>::digits / 2;
			uint128_t k = (UINT128_C(1) << i) - 1;
			while (i >= 8) {
			    x = ((x & k) << i) ^ ((x >> i) & k);
			    i >>= 1;
			    k ^= k << i;
			}
			return k;
		}

		// compile-time unit tests for byte swapping
		static_assert(byte_swap<uint16_t>(UINT16_C(0xABCD)) == UINT16_C(0xCDAB), "byte swap failed for u16");
		static_assert(byte_swap<uint32_t>(UINT32_C(0xABCDDCBA)) == UINT32_C(0xBADCCDAB), "byte swap failed for u32");
		static_assert(byte_swap<uint64_t>(UINT64_C(0xABCDDCBA12344321)) == UINT64_C(0x21433412BADCCDAB), "byte swap failed for u64");
		/// @}

		constexpr bool
		is_network_byte_order() noexcept {
			#if defined(WORDS_BIGENDIAN)
			return true;
			#else
			return false;
			#endif
		}
		template<class Int, bool intgeral>
		struct byte_swap_chooser {};

		template<class Int>
		struct byte_swap_chooser<Int, true> {

			inline void
			to_network_format() noexcept {
				if (!is_network_byte_order()) {
					_intval = byte_swap<Int>(_intval);
				}
			}

			inline void
			to_host_format() noexcept {
				this->to_network_format();
			}

			constexpr bool
			operator==(const byte_swap_chooser& rhs) const noexcept {
				return _intval == rhs._intval;
			}

		private:
			Int _intval; 
		};

		template<class Arr>
		struct byte_swap_chooser<Arr, false> {

			inline void
			to_network_format() noexcept {
				if (!is_network_byte_order()) {
					std::reverse(_arr, _arr + sizeof(Arr));
				}
			}

			inline void
			to_host_format() noexcept {
				this->to_network_format();
			}

			inline bool
			operator==(const byte_swap_chooser& rhs) const noexcept {
				return std::equal(_arr, _arr + sizeof(Arr), rhs._arr);
			}

		private:
			Arr _arr; 
		};

		template<size_t bytes>
		struct Integral: public byte_swap_chooser<uint8_t[bytes],false> {};
		template<> struct Integral<1>: public byte_swap_chooser<uint8_t,true> {};
		template<> struct Integral<2>: public byte_swap_chooser<uint16_t,true> {};
		template<> struct Integral<4>: public byte_swap_chooser<uint32_t,true> {};
		template<> struct Integral<8>: public byte_swap_chooser<uint64_t,true> {};
		template<> struct Integral<16>: public byte_swap_chooser<uint128_t,true> {};

		template<class T, class Ch=char>
		union Bytes {

			typedef Ch value_type;
			typedef Integral<sizeof(T)> integral_type;
			typedef value_type* iterator;
			typedef const value_type* const_iterator;
			typedef std::size_t size_type;

			constexpr
			Bytes() noexcept:
				_val{} {}

			constexpr
			Bytes(const Bytes& rhs) noexcept:
				_val(rhs._val) {}

			constexpr
			Bytes(T rhs) noexcept:
				_val(rhs) {}

			template<class It>
			Bytes(It first, It last) noexcept {
				std::copy(first, last, _bytes);
			}

			inline void
			to_network_format() noexcept {
				_intval.to_network_format();
			}

			inline void
			to_host_format() noexcept {
				_intval.to_host_format();
			}

			inline
			operator T&() noexcept {
				return _val;
			}

			constexpr
			operator const T&() const noexcept {
				return _val;
			}

			constexpr value_type
			operator[](size_type idx) const noexcept {
				return _bytes[idx];
			}

			constexpr bool
			operator==(const Bytes& rhs) const noexcept {
				return _intval == rhs._intval;
			}

			constexpr bool
			operator!=(const Bytes& rhs) const noexcept {
				return !operator==(rhs);
			}

			inline iterator
			begin() noexcept {
				return _bytes;
			}

			inline iterator
			end() noexcept {
				return _bytes + sizeof(T);
			}

			constexpr const_iterator
			begin() const noexcept {
				return _bytes;
			}

			constexpr const_iterator
			end() const noexcept {
				return _bytes + sizeof(T);
			}

			constexpr const T&
			value() const noexcept {
				return _val;
			}

			inline T&
			value() noexcept { return _val; }

			static constexpr size_type
			size() noexcept {
				return sizeof(T);
			}

		private:
			T _val;
			integral_type _intval;
			value_type _bytes[sizeof(T)];

			static_assert(sizeof(decltype(_val)) == sizeof(decltype(_intval)),
				"bad integral type");
		};

		template<class T>
		constexpr Bytes<T> make_bytes(T rhs) { return Bytes<T>(rhs); }

		template<class T, class B>
		std::ostream& operator<<(std::ostream& out, const Bytes<T,B>& rhs) {
			using components::intersperse_iterator;
			using components::Fixed_width;
			using components::use_flags;
			typedef typename Bytes<T,B>::value_type value_type;
			std::ostream::sentry s(out);
			if (s) {
				use_flags f(out, std::ios::hex, std::ios::basefield); 
				out.fill('0');
				intersperse_iterator<Fixed_width<unsigned int, 2>> it(out, " ");
				std::transform(rhs.begin(), rhs.end(), it,
					[] (value_type ch) -> unsigned char {
						return static_cast<unsigned char>(ch);
					}
				);
				out.fill(' ');
			}
			return out;
		}

	}

}
