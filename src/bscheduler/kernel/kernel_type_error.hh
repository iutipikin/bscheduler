#ifndef BSCHEDULER_KERNEL_KERNEL_TYPE_ERROR_HH
#define BSCHEDULER_KERNEL_KERNEL_TYPE_ERROR_HH

#include <bscheduler/base/error.hh>
#include <bscheduler/kernel/kernel_type.hh>

namespace bsc {

	class kenel_type_error: public error {

	private:
		kernel_type _tp1, _tp2;

	public:
		inline
		kenel_type_error(const kernel_type& tp1, const kernel_type& tp2):
		error("types have the same type indices/identifiers"),
		_tp1(tp1),
		_tp2(tp2)
		{}

		friend std::ostream&
		operator<<(std::ostream& out, const kenel_type_error& rhs);

	};

	std::ostream&
	operator<<(std::ostream& out, const kenel_type_error& rhs);

}

#endif // vim:filetype=cpp
