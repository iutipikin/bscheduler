#ifndef FACTORY_STDX_N_RANDOM_BYTES_HH
#define FACTORY_STDX_N_RANDOM_BYTES_HH

#include <algorithm>

namespace factory {
	
	namespace stdx {

		template<class Result, class Engine>
		Result n_random_bytes(Engine& engine) {
			typedef Result result_type;
			typedef typename Engine::result_type
				base_result_type;
			static_assert(sizeof(base_result_type) > 0 &&
				sizeof(result_type) > 0 &&
				sizeof(result_type) % sizeof(base_result_type) == 0,
				"bad result type");
			constexpr const std::size_t NUM_BASE_RESULTS =
				sizeof(result_type) / sizeof(base_result_type);
			union {
				result_type value{};
				base_result_type base[NUM_BASE_RESULTS];
			} result;
			std::generate_n(result.base,
				NUM_BASE_RESULTS,
				std::ref(engine));
			return result.value;
		}

	}

}

#endif // FACTORY_STDX_N_RANDOM_BYTES_HH