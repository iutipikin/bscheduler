#include "kernel_type_registry.hh"

#include <ostream>
#include <algorithm>

#include <bscheduler/kernel/kernel_error.hh>

namespace {

	struct Entry {
		Entry(const bsc::kernel_type& rhs): _type(rhs) {}

		inline friend std::ostream&
		operator<<(std::ostream& out, const Entry& rhs) {
			return out << "/type/" << rhs._type.id() << '/' << rhs._type;
		}

	private:
		const bsc::kernel_type& _type;
	};

}

bsc::kernel_type_registry::const_iterator
bsc::kernel_type_registry::find(id_type id) const noexcept {
	return std::find_if(
		this->begin(), this->end(),
		[&id] (const kernel_type& rhs) {
			return rhs.id() == id;
		}
	);
}

bsc::kernel_type_registry::const_iterator
bsc::kernel_type_registry::find(std::type_index idx) const noexcept {
	return std::find_if(
		this->begin(), this->end(),
		[&idx] (const kernel_type& rhs) {
			return rhs.index() == idx;
		}
	);
}

void
bsc::kernel_type_registry::register_type(kernel_type type) {
	const_iterator result;
	result = this->find(type.index());
	if (result != this->end()) {
		BSCHEDULER_THROW(kenel_type_error, type, *result);
	}
	if (type) {
		result = this->find(type.id());
		if (result != this->end()) {
			BSCHEDULER_THROW(kenel_type_error, type, *result);
		}
	} else {
		type.setid(this->generate_id());
	}
	this->_types.emplace_back(type);
}

std::ostream&
bsc::operator<<(std::ostream& out, const kernel_type_registry& rhs) {
	std::ostream_iterator<Entry> it(out, "\n");
	std::copy(rhs._types.begin(), rhs._types.end(), it);
	return out;
}

bsc::kernel*
bsc::kernel_type_registry::read_object(sys::pstream& packet) {
	id_type id;
	packet >> id;
	const_iterator result = this->find(id);
	if (result == this->end()) {
		throw kernel_error("unknown kernel type", id);
	}
	return result->read(packet);
}

bsc::kernel_type_registry bsc::types;
