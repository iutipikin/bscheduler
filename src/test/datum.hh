#ifndef TEST_DATUM_HH
#define TEST_DATUM_HH

#include <random>
#include <chrono>
#include <unistdx/net/bstream>

typedef std::chrono::nanoseconds::rep Time;

template<class T>
void rnd(T& val) {
	typedef typename
		std::conditional<std::is_floating_point<T>::value,
			std::uniform_real_distribution<T>,
				std::uniform_int_distribution<T>>::type
					Distribution;
	static std::default_random_engine generator;
	static Distribution distribution(
		std::numeric_limits<T>::min(),
		std::numeric_limits<T>::max()
	);
	val = distribution(generator);
}

struct Datum {

	Datum():
		x(0), y(0), z(0),
		u(0), v(0), w(0)
	{
		rnd(x); rnd(y); rnd(static_cast<float&>(z));
		rnd(static_cast<double&>(u)); rnd(v); rnd(w);
	}

	constexpr
	Datum(const Datum& rhs):
		x(rhs.x), y(rhs.y), z(rhs.z),
		u(rhs.u), v(rhs.v), w(rhs.w)
	{}

	Datum&
	operator=(const Datum&) = default;

	bool
	operator==(const Datum& rhs) const {
		return
			x == rhs.x && y == rhs.y && z == rhs.z &&
			u == rhs.u && v == rhs.v && w == rhs.w;
	}

	bool
	operator!=(const Datum& rhs) const {
		return
			x != rhs.x || y != rhs.y || z != rhs.z ||
			u != rhs.u || v != rhs.v || w != rhs.w;
	}

	friend sys::bstream&
	operator<<(sys::bstream& out, const Datum& rhs) {
		return out
			<< rhs.x << rhs.y << rhs.z
			<< rhs.u << rhs.v << rhs.w;
	}

	friend sys::bstream&
	operator>>(sys::bstream& in, Datum& rhs) {
		return in
			>> rhs.x >> rhs.y >> rhs.z
			>> rhs.u >> rhs.v >> rhs.w;
	}

	friend std::ostream&
	operator<<(std::ostream& out, const Datum& rhs) {
		write_raw(out, rhs.x);
		write_raw(out, rhs.y);
		write_raw(out, rhs.z);
		write_raw(out, rhs.u);
		write_raw(out, rhs.v);
		write_raw(out, rhs.w);
		return out;
	}

	friend std::istream&
	operator>>(std::istream& in, Datum& rhs) {
		read_raw(in, rhs.x);
		read_raw(in, rhs.y);
		read_raw(in, rhs.z);
		read_raw(in, rhs.u);
		read_raw(in, rhs.v);
		read_raw(in, rhs.w);
		return in;
	}

	constexpr static size_t real_size() {
		return
			sizeof(x) + sizeof(y) + sizeof(z) +
			sizeof(u) + sizeof(v) + sizeof(w);
	}

private:

	template<class T>
	static void
	write_raw(std::ostream& out, const T& rhs) {
		sys::bytes<T> raw = rhs;
		out.write(raw.begin(), raw.size());
	}

	template<class T>
	static void
	read_raw(std::istream& in, T& rhs) {
		sys::bytes<T> raw;
		in.read(raw.begin(), raw.size());
		rhs = raw;
	}

	int64_t x;
	int32_t y;
	sys::bytes<float> z;
	sys::bytes<double> u;
	int16_t v;
	int8_t w;
	char padding[5] = {};
};

#endif // vim:filetype=cpp
