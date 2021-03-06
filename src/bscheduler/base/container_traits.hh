#ifndef BSCHEDULER_BASE_CONTAINER_TRAITS_HH
#define BSCHEDULER_BASE_CONTAINER_TRAITS_HH

#include <stdexcept>

namespace bsc {

	/**
	\brief Container trais.
	\date 2018-05-22
	\author Ivan Gankevich
	\ingroup traits
	\details
	It is like character traits, but for containers, in a sense that
	you need to always carry traits type template parameter together with
	the container type template parameter. This type allows to use any
	container as a queue.
	*/
	template <class Container>
	struct container_traits {

		/// Container type
		typedef Container container_type;
		/// Container value type
		typedef typename Container::value_type value_type;

	};

	/**
	\brief Container traits for vector-like containers.
	\ingroup traits
	Method \c pop is not supported.
	*/
	template <class Container>
	struct vector_traits: public container_traits<Container> {

		using typename container_traits<Container>::container_type;
		using typename container_traits<Container>::value_type;

		/// Push element to the container.
		inline static void
		push(container_type& cnt, const value_type& rhs) {
			cnt.push_back(rhs);
		}

		/// Returns the first element in the container.
		inline static value_type&
		front(container_type& cnt) {
			return cnt.front();
		}

		/// Returns the first element in the container.
		inline static const value_type&
		front(const container_type& cnt) {
			return cnt.front();
		}

	};

	/**
	\brief Container traits for queue-like containers.
	\ingroup traits
	*/
	template <class Container>
	struct queue_traits: public container_traits<Container> {

		using typename container_traits<Container>::container_type;
		using typename container_traits<Container>::value_type;

		/// Push element to the container.
		inline static void
		push(container_type& cnt, const value_type& rhs) {
			cnt.push(rhs);
		}

		/// Returns the first element in the container.
		inline static value_type&
		front(container_type& cnt) {
			return cnt.front();
		}

		/// Returns the first element in the container.
		inline static const value_type&
		front(const container_type& cnt) {
			return cnt.front();
		}

		/// Removes the first element in the container.
		inline static void
		pop(container_type& cnt) {
			cnt.pop();
		}

	};

	/**
	\brief Container traits for priority queue container.
	\ingroup traits
	*/
	template <class Container>
	struct priority_queue_traits: public container_traits<Container> {

		using typename container_traits<Container>::container_type;
		using typename container_traits<Container>::value_type;

		/// Push element to the container.
		inline static void
		push(container_type& cnt, const value_type& rhs) {
			cnt.push(rhs);
		}

		/// Returns the first element in the container.
		inline static const value_type&
		front(const container_type& cnt) {
			return cnt.top();
		}

		/// Removes the first element in the container.
		inline static void
		pop(container_type& cnt) {
			cnt.pop();
		}

	};

	/**
	\brief Container traits for deque container.
	\ingroup traits
	*/
	template <class Container>
	struct deque_traits: public container_traits<Container> {

		using typename container_traits<Container>::container_type;
		using typename container_traits<Container>::value_type;

		/// Push element to the container.
		inline static void
		push(container_type& cnt, const value_type& rhs) {
			cnt.push_back(rhs);
		}

		/// Returns the first element in the container.
		inline static value_type&
		front(container_type& cnt) {
			return cnt.front();
		}

		/// Returns the first element in the container.
		inline static const value_type&
		front(const container_type& cnt) {
			return cnt.front();
		}

		/// Removes the first element in the container.
		inline static void
		pop(container_type& cnt) {
			cnt.pop_front();
		}

	};

}

#endif // vim:filetype=cpp
