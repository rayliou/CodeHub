#include <coroutine>
#include <future>
#include <ev.h>
#include <memory>
#include <unistd.h>
#include "logger.h"

static auto &logger = Logger::getLogger(__FILE__);
LogLevel Logger::logLevel{LogLevel::TRACE};

struct ASyncTask {
    struct promise_type {
        ASyncTask get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
    //Implement the await behavior
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> awaiting_coro) noexcept {
        awaiting_coro.resume();
    }
    void await_resume() const noexcept {}
};

class TimerAwaitable {
public:
    TimerAwaitable(struct ev_loop* loop, double timeout)
        : loop_(loop), timeout_(timeout), handle_(nullptr) {
        ev_timer_init(&timer_watcher_, TimerAwaitable::timer_callback, timeout_, 0.);
        timer_watcher_.data = this;
        logger.trace("%s ev_timer_init %f", __FUNCTION__,timeout_);
    }
    ~ TimerAwaitable()
    {
        logger.trace("%s ev_timer_stop timeout %f", __FUNCTION__,timeout_);
        ev_timer_stop(loop_, &timer_watcher_);
    }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        handle_ = handle;
        ev_timer_set(&timer_watcher_, timeout_, 0.);
        ev_timer_start(loop_, &timer_watcher_);
        logger.debug(" %s ev_timer_start", __FUNCTION__);
    }

    void await_resume() noexcept {
        logger.trace("%s timer resume %f", __FUNCTION__,timeout_);
    }

    static void timer_callback(struct ev_loop* loop, ev_timer* watcher, int revents) {
        TimerAwaitable* self = static_cast<TimerAwaitable*>(watcher->data);
        self->handle_.resume();
    }

private:
    struct ev_loop* loop_;
    ev_timer timer_watcher_;
    double timeout_;
    std::coroutine_handle<> handle_;
};

class StdinAwaitable {
public:
    StdinAwaitable(struct ev_loop* loop)
        : loop_(loop), handle_(nullptr) {
        ev_io_init(&stdin_watcher_, StdinAwaitable::stdin_callback, STDIN_FILENO, EV_READ);
        stdin_watcher_.data = this;
    }
    ~ StdinAwaitable()
    {
        logger.debug("%s", __FUNCTION__);
    }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        handle_ = handle;
        ev_io_start(loop_, &stdin_watcher_);
    }

    std::string await_resume() noexcept {
        char buffer[1024];
        ssize_t nread = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
        if (nread > 0) {
            buffer[nread] = '\0';  // 添加字符串结束符
            return buffer;
        }
        return "";
    }

    static void stdin_callback(struct ev_loop* loop, ev_io* watcher, int revents) {
        StdinAwaitable* self = static_cast<StdinAwaitable*>(watcher->data);
        ev_io_stop(loop, watcher);  // 标准输入读取后，停止 watcher
        self->handle_.resume();
    }

private:
    struct ev_loop* loop_;
    ev_io stdin_watcher_;
    std::coroutine_handle<> handle_;
};

static TimerAwaitable async_sleep(double secs, struct ev_loop* loop) { return TimerAwaitable(loop, secs); }
static StdinAwaitable async_read(struct ev_loop* loop) { return StdinAwaitable(loop); }


static ASyncTask  _alternating_timers(struct ev_loop* loop) {
    TimerAwaitable t(loop,2);
    TimerAwaitable t5(loop,5);
    for(int i= 0; i < 3; i++) {
        logger.info("Waiting for timer 1...");
        co_await t; //reuse
        co_await t5; //reuse
        //co_await async_sleep(1,loop);
    }
}

static ASyncTask alternating_timers(struct ev_loop* loop) {
    co_await _alternating_timers(loop);
}

static ASyncTask stdin_and_timer(struct ev_loop* loop) {
    //TimerAwaitable timer(loop, 5.0);  // 5秒定时器

    while (true) {
        auto v = co_await async_read(loop);
        logger.info("Read Value %s", v.c_str());
        //co_await timer;  // 等待定时器
        co_await alternating_timers(loop);
    }
}
class A 
{
public:
    ASyncTask init(struct ev_loop* loop)
    {
        co_await alternating_timers(loop);
        auto v = co_await async_read(loop);
        logger.info("Read Value %s", v.c_str());

    }
};

int main() {
    struct ev_loop* loop = EV_DEFAULT;
    //alternating_timers(loop);
    //stdin_and_timer(loop);
    A a;
    a.init(loop);
    ev_run(loop, 0);

    return 0;
}

