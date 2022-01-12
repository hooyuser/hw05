#pragma once

#include <shared_mutex>
#include <utility>
#include <optional>
#include <queue>


#define FWD(...) ::std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

template <typename T> class Mutex {
public:
	decltype(auto) with_lock(std::invocable<T> auto&& f) {
		std::lock_guard lock{ m_mutex };
		return std::invoke(FWD(f), m_value);
	}

	decltype(auto) with_lock(std::invocable<T> auto&& f) const {
		std::shared_lock lock{ m_mutex };
		return std::invoke(FWD(f), m_value);
	}

protected:
	T m_value;
	mutable std::shared_mutex m_mutex;
};

template <typename T> class ConditionVariable : public Mutex<T> {
public:
	void notifyOne() { m_cond.notify_one(); }
	void notifyAll() { m_cond.notify_all(); }

	void wait(std::invocable<T> auto f) {
		auto lock = std::unique_lock{ this->m_mutex };
		m_cond.wait(lock, [&] { return std::invoke(f, this->m_value); });
	}

	void wait(std::invocable<T> auto f, std::stop_token st) {
		auto lock = std::unique_lock{ this->m_mutex };
		m_cond.wait(lock, st, [&] { return std::invoke(f, this->m_value); });
	}

private:
	std::condition_variable_any m_cond;
};

template <typename T> class ThreadSafeQueue {
public:
	template <typename Arg> requires std::constructible_from<T, Arg>
	void emplace(Arg&& args) {
		m_queue.with_lock([&](auto&& queue) { queue.emplace(FWD(args)); });
		m_queue.notifyOne();
	}

	template <typename ...Args> requires std::constructible_from<T, Args...>
	void emplace(Args&&... args) {
		m_queue.with_lock([&](auto&& queue) { queue.emplace(FWD(args)...); });
		m_queue.notifyAll();
	}

	std::optional<T> pop() {
		std::optional<T> ele;
		m_queue.with_lock([&](auto&& queue) {
			if (!queue.empty()) {
				ele.emplace(std::move(queue.front()));
				queue.pop();
			}
			});
		return ele;
	}

	void waitForAnElement(std::stop_token st) {
		auto hasElement = [](const auto& queue) { return !queue.empty(); };
		m_queue.wait(hasElement, st);
	}

private:
	ConditionVariable<std::queue<T>> m_queue;
};