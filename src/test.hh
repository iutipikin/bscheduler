namespace test {

	template<class T>
	std::basic_string<T> random_string(size_t size, T min = std::numeric_limits<T>::min(),
		T max = std::numeric_limits<T>::max()) {
		static std::default_random_engine generator(
			std::chrono::high_resolution_clock::now().time_since_epoch().count()
		);
		std::uniform_int_distribution<T> dist(min, max);
		auto gen = std::bind(dist, generator);
		std::basic_string<T> ret(size, '_');
		std::generate(ret.begin(), ret.end(), gen);
		return ret;
	}

	template<class X, class Y>
	void equal(X x, Y y) {
		if (!(x == y)) {
			std::stringstream msg;
			msg << "values are not equal: x="
				<< x << ", y=" << y;
			throw std::runtime_error(msg.str());
		}
	}

	template<class Container1, class Container2>
	void compare(Container1 cnt1, Container2 cnt2) {
		using namespace factory;
		auto pair = std::mismatch(cnt1.begin(), cnt1.end(), cnt2.begin());
		if (pair.first != cnt1.end()) {
			auto pos = pair.first - cnt1.begin();
			std::stringstream msg;
			msg << "input and output does not match at i=" << pos << ":\n'"
				<< make_bytes(*pair.first) << "'\n!=\n'" << make_bytes(*pair.second) << "'";
			throw Error(msg.str(), __FILE__, __LINE__, __func__);
		}
	}

}

namespace std {
	std::ostream& operator<<(std::ostream& out, const std::basic_string<unsigned char>& rhs) {
		std::ostream_iterator<char> it(out, "");
		std::copy(rhs.begin(), rhs.end(), it);
		return out;
	}
}
