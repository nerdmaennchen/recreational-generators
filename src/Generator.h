#include <ucontext.h>

#include <iterator>
#include <optional>
#include <memory>
#include <functional>

template<typename T>
struct Yielder {
protected:
	std::optional<T> val;
	ucontext_t& caller_context;
	ucontext_t& callee_context;
	Yielder(ucontext_t& _caller_context, ucontext_t& _callee_context)
		: caller_context{_caller_context}
		, callee_context{_callee_context}
	{}

public:
	template<typename... Args>
	void yield(Args &&... args) {
		val.emplace(std::forward<Args>(args)...);
		swapcontext(&callee_context, &caller_context);
	}

	auto operator*() -> T& {
		return val.operator*();
	}

	auto operator->() -> T& {
		return val.operator->();
	}

	auto operator*() const -> T const& {
		return val.operator*();
	}

	auto operator->() const -> T const* {
		return val.operator->();
	}
};

template<typename T, int stack_size=4096>
struct Generator {
private:
	std::function<void(Yielder<T>&)> func;
public:
	struct end_iterator {};
	struct iterator : Yielder<T>, std::iterator<std::forward_iterator_tag, T> {
		using super = std::iterator<std::forward_iterator_tag, T>;
		using value_type = typename super::value_type;
		using reference = typename super::reference;
		using pointer = typename super::reference;

	private:
		std::function<void(Yielder<T>&)> func;
		std::array<std::byte, stack_size> stack{};

		ucontext_t caller_context;
		ucontext_t callee_context;
		ucontext_t exit_context;
	public:
		static void call(iterator* iter) {
			iter->func(*iter);
		}
		static void exit(iterator* iter) {
			iter->val.reset();
		}
		iterator(std::function<void(Yielder<T>&)>&& f) : Yielder<T>{caller_context, callee_context}, func{std::move(f)} {
    		getcontext(&caller_context);
    		getcontext(&callee_context);
    		getcontext(&exit_context);
			callee_context.uc_stack.ss_sp   = stack.data();
			callee_context.uc_stack.ss_size = stack.size();
			exit_context.uc_stack.ss_sp     = stack.data();
			exit_context.uc_stack.ss_size   = stack.size();
			callee_context.uc_link          = &exit_context;

			makecontext(&callee_context, reinterpret_cast<void(*)()>(call), 1, this);
			makecontext(&exit_context, reinterpret_cast<void(*)()>(exit), 1, this);
			++(*this);
		}

		iterator& operator++() {
			swapcontext(&caller_context, &callee_context);
			return *this;
		}

		template<typename Other>
		bool operator!=(Other const&) const {
			return this->val.has_value();
		}
	};

	template<typename Functor>
	Generator(Functor&& f) : func{std::forward<Functor>(f)} {}

	iterator begin() {
		return iterator(std::move(func));
	}
	end_iterator end() {
		return {};
	}
};