#ifndef UNISTDX_IT_QUEUE_PUSHER
#define UNISTDX_IT_QUEUE_PUSHER

#include <iterator>

#include <bscheduler/base/container_traits.hh>

namespace bsc {

	namespace bits {

		typedef std::iterator<std::output_iterator_tag, void, void, void, void>
			queue_push_iterator_base;

	}

	/**
	\brief Output iterator that inserts element to the container on assignment.
	\date 2018-05-22
	\author Ivan Gankevich
	\ingroup iter
	\see queue_pop_iterator
	\tparam Container container type
	\tparam Traits container traits type
	*/
	template <class Container, class Traits = queue_traits<Container>>
	class queue_push_iterator: public bits::queue_push_iterator_base {

	private:
		typedef typename Container::value_type cont_value_type;
		typedef bits::queue_push_iterator_base base_type;

	public:
		using typename base_type::iterator_category;
		using typename base_type::value_type;
		using typename base_type::pointer;
		using typename base_type::reference;
		using typename base_type::difference_type;

	public:
		/// Container type.
		typedef Container container_type;
		/// Container traits type.
		typedef Traits traits_type;
		/// Container element type.
		typedef cont_value_type object_type;

	private:
		Container& _container;

	public:

		/// Construct queue push iterator from container \p x.
		explicit inline
		queue_push_iterator(Container& x) noexcept: _container(x) {}

		/// Push element to the container.
		inline queue_push_iterator&
		operator=(const object_type& rhs) {
			traits_type::push(this->_container, rhs);
			return *this;
		}

		/// Move element to the container.
		inline queue_push_iterator&
		operator=(const object_type&& rhs) {
			traits_type::push(this->_container, std::move(rhs));
			return *this;
		}

		/// Does nothing.
		inline queue_push_iterator&
		operator*() noexcept {
			return *this;
		}

		/// Does nothing.
		inline queue_push_iterator&
		operator++() noexcept {
			return *this;
		}

		/// Does nothing.
		inline queue_push_iterator
		operator++(int) noexcept {
			return *this;
		}

	};

	/**
	\brief Construct \link queue_push_iterator\endlink
	which inserts elements to container \p rhs.
	\details This iterator uses \link queue_traits\endlink.
	*/
	template<class C>
	inline queue_push_iterator<C>
	queue_pusher(C& rhs) {
		return queue_push_iterator<C>(rhs);
	}

	/**
	\copybrief queue_pusher
	\details This iterator uses \link priority_queue_traits\endlink.
	*/
	template<class C>
	inline queue_push_iterator<C,priority_queue_traits<C>>
	priority_queue_pusher(C& rhs) {
		return queue_push_iterator<C,priority_queue_traits<C>>(rhs);
	}

	/**
	\copybrief queue_pusher
	\details This iterator uses \link deque_traits\endlink.
	*/
	template<class C>
	inline queue_push_iterator<C,deque_traits<C>>
	deque_pusher(C& rhs) {
		return queue_push_iterator<C,deque_traits<C>>(rhs);
	}

}

#endif // vim:filetype=cpp
