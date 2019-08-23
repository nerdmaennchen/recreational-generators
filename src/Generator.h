#include <ucontext.h>

#include <iterator>
#include <optional>
#include <functional>
#include <memory>
#include <future>
#include <exception>
#include <algorithm>

template<typename T>
struct Yielder {
protected:
    std::optional<T> val;
    ucontext_t& caller_context;
    ucontext_t& callee_context;
    std::exception_ptr eptr{};

    Yielder(ucontext_t& _caller_context, ucontext_t& _callee_context)
        : caller_context{_caller_context}
        , callee_context{_callee_context}
    {}

public:
    template<typename... Args>
    void yield(Args &&... args) {
        val.emplace(std::forward<Args>(args)...);
        swapcontext(&callee_context, &caller_context);
        if (eptr) {
            std::rethrow_exception(eptr);
        }
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
        struct CleanupStack : std::exception {};

        std::function<void(Yielder<T>&)> func;
        std::array<std::byte, stack_size> stack;

        ucontext_t caller_context;
        ucontext_t callee_context;

        bool returned {false};
        static void call(iterator* iter) noexcept {
            try {
                iter->func(*iter);
                iter->returned = true;
            } catch (CleanupStack const&) {}
            iter->val.reset();
        }
    public:
        iterator(std::function<void(Yielder<T>&)>&& f) : Yielder<T>{caller_context, callee_context}, func{std::move(f)} {
            getcontext(&caller_context);
            getcontext(&callee_context);
            callee_context.uc_stack.ss_sp   = stack.data();
            callee_context.uc_stack.ss_size = stack.size();
            callee_context.uc_link = &caller_context;
            makecontext(&callee_context, reinterpret_cast<void(*)()>(call), 1, this);
            ++(*this);
        }

        ~iterator() {
            if (not returned) {
                // in this case the generator context has to be torn down because it cannot be continued to its end
                // setting the eptr will rethrow the exception the next time just before the function is continued thus tearing down the callees stack
                try {
                    throw CleanupStack{};
                } catch (...) {
                    this->eptr = std::current_exception();                                
                }
                ++(*this);
            }
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
