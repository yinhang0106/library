#include <chrono>
#include <coroutine>
#include <deque>
#include <queue>
#include <span>
#include <thread>
#include "rbtree.h"
#include <debugger.h>

using namespace std::chrono_literals;

template<class T = void>
struct NonVoidHelper {
    using Type = T;
};

template<>
struct NonVoidHelper<void> {
    using Type = NonVoidHelper;

    explicit NonVoidHelper() = default;
};

template<class T>
struct Uninitialized {
    union {
        T mValue;
    };

    Uninitialized() noexcept {
    }

    Uninitialized(Uninitialized &&) = delete;

    ~Uninitialized() noexcept {
    }

    T moveValue() {
        T ret(std::move(mValue));
        mValue.~T();
        return ret;
    }

    template<class... Ts>
    void putValue(Ts &&... args) {
        new(std::addressof(mValue)) T(std::forward<Ts>(args)...);
    }
};

template<>
struct Uninitialized<void> {
    auto moveValue() {
        return NonVoidHelper<>{};
    }

    void putValue(NonVoidHelper<>) {
    }
};

template<class T>
struct Uninitialized<T const> : Uninitialized<T> {
};

template<class T>
struct Uninitialized<T &> : Uninitialized<std::reference_wrapper<T> > {
};

template<class T>
struct Uninitialized<T &&> : Uninitialized<T> {
};

template<class A>
concept Awaiter = requires(A a, std::coroutine_handle<> h)
{
    { a.await_ready() };
    { a.await_suspend(h) };
    { a.await_resume() };
};

template<class A>
concept Awaitable = Awaiter<A> || requires(A a)
{
    { a.operator co_await() } -> Awaiter;
};

template<class A>
struct AwaitableTraits;

template<Awaiter A>
struct AwaitableTraits<A> {
    using RetType = decltype(std::declval<A>().await_resume());
    using NonVoidRetType = NonVoidHelper<RetType>::Type;
};

template<class A>
    requires(!Awaiter<A> && Awaitable<A>)
struct AwaitableTraits<A>
        : AwaitableTraits<decltype(std::declval<A>().operator co_await())> {
};

template<class To, std::derived_from<To> P>
constexpr std::coroutine_handle<To> staticHandleCast(std::coroutine_handle<P> coroutine) {
    return std::coroutine_handle<To>::from_address(coroutine.address());
}

struct RepeatAwaiter {
    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) const noexcept {
        if (coroutine.done())
            return std::noop_coroutine();
        else
            return coroutine;
    }

    void await_resume() const noexcept {
    }
};

struct PreviousAwaiter {
    std::coroutine_handle<> mPrevious;

    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) const noexcept {
        if (mPrevious)
            return mPrevious;
        else
            return std::noop_coroutine();
    }

    void await_resume() const noexcept {
    }
};

template<class T>
struct Promise {
    auto initial_suspend() noexcept {
        return std::suspend_always();
    }

    auto final_suspend() noexcept {
        return PreviousAwaiter(mPrevious);
    }

    void unhandled_exception() noexcept {
        mException = std::current_exception();
    }

    void return_value(T &&ret) {
        mResult.putValue(std::move(ret));
    }

    void return_value(T const &ret) {
        mResult.putValue(ret);
    }

    T result() {
        if (mException) [[unlikely]] {
            std::rethrow_exception(mException);
        }
        return mResult.moveValue();
    }

    auto get_return_object() {
        return std::coroutine_handle<Promise>::from_promise(*this);
    }

    std::coroutine_handle<> mPrevious{};
    std::exception_ptr mException{};
    Uninitialized<T> mResult;

    Promise &operator=(Promise &&) = delete;
};

template<>
struct Promise<void> {
    auto initial_suspend() noexcept {
        return std::suspend_always();
    }

    auto final_suspend() noexcept {
        return PreviousAwaiter(mPrevious);
    }

    void unhandled_exception() noexcept {
        mException = std::current_exception();
    }

    void return_void() noexcept {
    }

    void result() {
        if (mException) [[unlikely]] {
            std::rethrow_exception(mException);
        }
    }

    auto get_return_object() {
        return std::coroutine_handle<Promise>::from_promise(*this);
    }

    std::coroutine_handle<> mPrevious{};
    std::exception_ptr mException{};

    Promise &operator=(Promise &&) = delete;
};

template<class T = void, class P = Promise<T> >
struct Task {
    using promise_type = P;

    Task(std::coroutine_handle<promise_type> coroutine) noexcept
        : mCoroutine(coroutine) {
    }

    Task(Task &&) = delete;

    ~Task() {
        mCoroutine.destroy();
    }

    struct Awaiter {
        bool await_ready() const noexcept {
            return false;
        }

        std::coroutine_handle<promise_type>
        await_suspend(std::coroutine_handle<> coroutine) const noexcept {
            mCoroutine.promise().mPrevious = coroutine;
            return mCoroutine;
        }

        T await_resume() const {
            return mCoroutine.promise().result();
        }

        std::coroutine_handle<promise_type> mCoroutine;
    };

    auto operator co_await() const noexcept {
        return Awaiter(mCoroutine);
    }

    operator std::coroutine_handle<>() const noexcept {
        return mCoroutine;
    }

    std::coroutine_handle<promise_type> mCoroutine;
};

struct SleepUntilPromise : RbTree<SleepUntilPromise>::RbNode, Promise<void> {
    std::chrono::system_clock::time_point mExpireTime;

    auto get_return_object() {
        return std::coroutine_handle<SleepUntilPromise>::from_promise(*this);
    }

    SleepUntilPromise &operator=(SleepUntilPromise &&) = delete;

    friend bool operator<(SleepUntilPromise const &lhs, SleepUntilPromise const &rhs) noexcept {
        return lhs.mExpireTime < rhs.mExpireTime;
    }
};

struct Loop {
    RbTree<SleepUntilPromise> mRbTimer{};

    void addTimer(SleepUntilPromise &promise) {
        mRbTimer.insert(promise);
    }

    void run(std::coroutine_handle<> coroutine) {
        while (!coroutine.done()) {
            coroutine.resume();
            while (!mRbTimer.empty()) {
                if (!mRbTimer.empty()) {
                    auto nowTime = std::chrono::system_clock::now();
                    auto &promise = mRbTimer.front();
                    if (promise.mExpireTime < nowTime) {
                        mRbTimer.erase(promise);
                        std::coroutine_handle<SleepUntilPromise>::from_promise(promise).resume();
                    } else {
                        std::this_thread::sleep_until(promise.mExpireTime);
                    }
                }
            }
        }
    }

    Loop &operator=(Loop &&) = delete;
};

Loop &getLoop() {
    static Loop loop;
    return loop;
}

struct SleepAwaiter {
    bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<SleepUntilPromise> coroutine) const {
        auto &promise = coroutine.promise();
        promise.mExpireTime = mExpireTime;
        loop.addTimer(promise);
    }

    void await_resume() const noexcept {
    }

    Loop &loop;
    std::chrono::system_clock::time_point mExpireTime;
};

Task<void, SleepUntilPromise> sleep_until(std::chrono::system_clock::time_point expireTime) {
    auto &loop = getLoop();
    co_await SleepAwaiter(loop, expireTime);
}

Task<void, SleepUntilPromise> sleep_for(std::chrono::system_clock::duration duration) {
    auto &loop = getLoop();
    co_await SleepAwaiter(loop, std::chrono::system_clock::now() + duration);
}

struct CurrentCoroutineAwaiter {
    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) noexcept {
        mCurrent = coroutine;
        return coroutine;
    }

    auto await_resume() const noexcept {
        return mCurrent;
    }

    std::coroutine_handle<> mCurrent;
};

struct ReturnPreviousPromise {
    auto initial_suspend() noexcept {
        return std::suspend_always();
    }

    auto final_suspend() noexcept {
        return PreviousAwaiter(mPrevious);
    }

    void unhandled_exception() {
        throw;
    }

    void return_value(std::coroutine_handle<> previous) noexcept {
        mPrevious = previous;
    }

    auto get_return_object() {
        return std::coroutine_handle<ReturnPreviousPromise>::from_promise(
            *this);
    }

    std::coroutine_handle<> mPrevious{};

    ReturnPreviousPromise &operator=(ReturnPreviousPromise &&) = delete;
};

struct ReturnPreviousTask {
    using promise_type = ReturnPreviousPromise;

    ReturnPreviousTask(std::coroutine_handle<promise_type> coroutine) noexcept
        : mCoroutine(coroutine) {
    }

    ReturnPreviousTask(ReturnPreviousTask &&) = delete;

    ~ReturnPreviousTask() {
        mCoroutine.destroy();
    }

    std::coroutine_handle<promise_type> mCoroutine;
};

struct WhenAllCtlBlock {
    std::size_t mCount;
    std::coroutine_handle<> mPrevious{};
    std::exception_ptr mException{};
};

struct WhenAllAwaiter {
    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) const {
        if (mTasks.empty()) return coroutine;
        mControl.mPrevious = coroutine;
        for (auto const &t: mTasks.subspan(0, mTasks.size() - 1))
            t.mCoroutine.resume();
        return mTasks.back().mCoroutine;
    }

    void await_resume() const {
        if (mControl.mException) [[unlikely]] {
            std::rethrow_exception(mControl.mException);
        }
    }

    WhenAllCtlBlock &mControl;
    std::span<ReturnPreviousTask const> mTasks;
};

template<class T>
ReturnPreviousTask whenAllHelper(auto const &t, WhenAllCtlBlock &control,
                                 Uninitialized<T> &result) {
    try {
        result.putValue(co_await t);
    } catch (...) {
        control.mException = std::current_exception();
        co_return control.mPrevious;
    }
    --control.mCount;
    if (control.mCount == 0) {
        co_return control.mPrevious;
    }
    co_return nullptr;
}

template<std::size_t... Is, class... Ts>
Task<std::tuple<typename AwaitableTraits<Ts>::NonVoidRetType...> >
whenAllImpl(std::index_sequence<Is...>, Ts &&... ts) {
    WhenAllCtlBlock control{sizeof...(Ts)};
    std::tuple<Uninitialized<typename AwaitableTraits<Ts>::RetType>...> result;
    ReturnPreviousTask taskArray[]{whenAllHelper(ts, control, std::get<Is>(result))...};
    co_await WhenAllAwaiter(control, taskArray);
    co_return std::tuple<typename AwaitableTraits<Ts>::NonVoidRetType...>(
        std::get<Is>(result).moveValue()...);
}

template<Awaitable... Ts>
    requires(sizeof...(Ts) != 0)
auto when_all(Ts &&... ts) {
    return whenAllImpl(std::make_index_sequence<sizeof...(Ts)>{},
                       std::forward<Ts>(ts)...);
}

struct WhenAnyCtlBlock {
    static constexpr std::size_t kNullIndex = std::size_t(-1);

    std::size_t mIndex{kNullIndex};
    std::coroutine_handle<> mPrevious{};
    std::exception_ptr mException{};
};

struct WhenAnyAwaiter {
    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) const {
        if (mTasks.empty()) return coroutine;
        mControl.mPrevious = coroutine;
        for (auto const &t: mTasks.subspan(0, mTasks.size() - 1))
            t.mCoroutine.resume();
        return mTasks.back().mCoroutine;
    }

    void await_resume() const {
        if (mControl.mException) [[unlikely]] {
            std::rethrow_exception(mControl.mException);
        }
    }

    WhenAnyCtlBlock &mControl;
    std::span<ReturnPreviousTask const> mTasks;
};

template<class T>
ReturnPreviousTask whenAnyHelper(auto const &t, WhenAnyCtlBlock &control,
                                 Uninitialized<T> &result, std::size_t index) {
    try {
        result.putValue(co_await t);
    } catch (...) {
        control.mException = std::current_exception();
        co_return control.mPrevious;
    }
    --control.mIndex = index;
    co_return control.mPrevious;
}

template<std::size_t... Is, class... Ts>
Task<std::variant<typename AwaitableTraits<Ts>::NonVoidRetType...> >
whenAnyImpl(std::index_sequence<Is...>, Ts &&... ts) {
    WhenAnyCtlBlock control{};
    std::tuple<Uninitialized<typename AwaitableTraits<Ts>::RetType>...> result;
    ReturnPreviousTask taskArray[]{whenAnyHelper(ts, control, std::get<Is>(result), Is)...};
    co_await WhenAnyAwaiter(control, taskArray);
    Uninitialized<std::variant<typename AwaitableTraits<Ts>::NonVoidRetType...> > varResult;
    ((control.mIndex == Is && (varResult.putValue(
                                   std::in_place_index<Is>, std::get<Is>(result).moveValue()), 0)), ...);
    co_return varResult.moveValue();
}

template<Awaitable... Ts>
    requires(sizeof...(Ts) != 0)
auto when_any(Ts &&... ts) {
    return whenAnyImpl(std::make_index_sequence<sizeof...(Ts)>{},
                       std::forward<Ts>(ts)...);
}

Task<int> hello1() {
    debug(), "hello1开始睡1秒";
    co_await sleep_for(1s); // 1s 等价于 std::chrono::seconds(1)
    debug(), "hello1睡醒了";
    co_return 1;
}

Task<int> hello2() {
    debug(), "hello2开始睡2秒";
    co_await sleep_for(2s); // 2s 等价于 std::chrono::seconds(2)
    debug(), "hello2睡醒了";
    co_return 2;
}

Task<int> hello() {
    debug(), "hello开始等1和2";
    auto v = co_await when_any(hello1(), hello2());
    /* co_await hello1(); */
    /* co_await hello2(); */
    debug(), "hello看到", (int) v.index() + 1, "睡醒了";
    co_return std::get<0>(v);
}

int main() {
    auto t = hello();
    getLoop().run(t);
    debug(), "主函数中得到hello结果:", t.mCoroutine.promise().result();
    return 0;
}
