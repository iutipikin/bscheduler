#ifndef BSCHEDULER_BASE_QUEUE_POPPER_HH
#define BSCHEDULER_BASE_QUEUE_POPPER_HH

#include <iterator>

#include <bscheduler/base/container_traits.hh>

namespace bsc {

	/**
	\brief Input iterator that removes element from the container on increment.
	\date 2018-05-22
	\author Ivan Gankevich
	\ingroup iter
	\see queue_push_iterator
	\tparam Container container type
	\tparam Traits container traits type
	*/
	template <class Container, class Traits=queue_traits<Container>>
	class queue_pop_iterator:
		public std::iterator<std::input_iterator_tag, typename Container::value_type>{

	private:
		typedef typename Container::value_type cont_value_type;
		typedef std::iterator<std::input_iterator_tag, cont_value_type>
			base_type;

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

	private:
		container_type* _container = nullptr;

	public:

		/// Construct queue pop iterator from container \p x.
		inline explicit constexpr
		queue_pop_iterator(container_type& x) noexcept:
		_container(&x) {}

		inline constexpr
		queue_pop_iterator() noexcept = default;

		/// Copy-constructor.
		inline constexpr
		queue_pop_iterator(const queue_pop_iterator& rhs) noexcept = default;

		/**
		\brief Returns true, if the container is empty.
		\details
		Strictly speaking, the implementation is incorrect, but it works
		in all cases this iterator was designed for.
		*/
		inline bool
		operator==(const queue_pop_iterator&) const noexcept {
			return this->_container->empty();
		}

		/**
		\brief Returns true, if the container is not empty.
		\see operator==
		*/
		inline bool
		operator!=(const queue_pop_iterator& rhs) const noexcept {
			return !this->operator==(rhs);
		}

		/// Dereference.
		inline const value_type&
		operator*() const noexcept {
			return traits_type::front(*this->_container);
		}

		/// Dereference.
		inline const value_type*
		operator->() const noexcept {
			return &traits_type::front(*this->_container);
		}

		/// Remove element from the container.
		inline queue_pop_iterator&
		operator++() noexcept {
			traits_type::pop(*this->_container);
			return *this;
		}

		/**
		\brief Impossible to implement
		\throws std::logic_error
		*/
		inline queue_pop_iterator&
		operator++(int) {
			throw
			std::logic_error("can not post increment bsc::queue_pop_iterator");
			return *this;
		}

	};

	/**
	\brief Construct \link queue_pop_iterator\endlink
	which "points" to the beginning of container \p cont.
	\details This iterator uses \link queue_traits\endlink.
	*/
	template<class C>
	inline queue_pop_iterator<C>
	queue_popper(C& cont) noexcept {
		return queue_pop_iterator<C>(cont);
	}

	/**
	\brief Construct \link queue_pop_iterator\endlink
	which "points" to the end of container.
	\details This iterator uses \link queue_traits\endlink.
	*/
	template<class C>
	inline queue_pop_iterator<C>
	queue_popper_end(C&) noexcept {
		return queue_pop_iterator<C>();
	}

	/**
	\copybrief queue_popper
	\details This iterator uses \link priority_queue_traits\endlink.
	*/
	template<class C>
	inline queue_pop_iterator<C,priority_queue_traits<C>>
	priority_queue_popper(C& cont) noexcept {
		return queue_pop_iterator<C,priority_queue_traits<C>>(cont);
	}

	/**
	\copybrief queue_popper_end
	\details This iterator uses \link priority_queue_traits\endlink.
	*/
	template<class C>
	inline queue_pop_iterator<C,priority_queue_traits<C>>
	priority_queue_popper_end(C&) noexcept {
		return queue_pop_iterator<C,priority_queue_traits<C>>();
	}

	/**
	\copybrief queue_popper
	\details This iterator uses \link deque_traits\endlink.
	*/
	template<class C>
	inline queue_pop_iterator<C,deque_traits<C>>
	deque_popper(C& cont) noexcept {
		return queue_pop_iterator<C,deque_traits<C>>(cont);
	}

	/**
	\copybrief queue_popper_end
	\details This iterator uses \link deque_traits\endlink.
	*/
	template<class C>
	inline queue_pop_iterator<C,deque_traits<C>>
	deque_popper_end(C&) noexcept {
		return queue_pop_iterator<C,deque_traits<C>>();
	}

}

#endif // vim:filetype=cpp
