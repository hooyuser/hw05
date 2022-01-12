#pragma once

#include <shared_mutex>
#include <functional>
#include <utility>
#include <optional>
#include <queue>

#define FWD(...) ::std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

template <typename T> class Mutex {
public:
	template <typename F>
	decltype(auto) with_lock(F&& f) {
		std::lock_guard lock{ m_mutex };
		return std::invoke(FWD(f), m_value);
	}
	template <typename F>
	decltype(auto) with_lock(F&& f) const {
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

	template <typename F>
	void wait(F f) {
		auto lock = std::unique_lock{ this->m_mutex };
		m_cond.wait(lock, [&, this] { return std::invoke(f, this->m_value); });
	}

	template <typename F>
	void wait(F f, std::stop_token st) {
		auto lock = std::unique_lock{ this->m_mutex };
		m_cond.wait(lock, st, [&, this] { return std::invoke(f, this->m_value); });
	}

private:
	std::condition_variable_any m_cond;
};

template <typename T> class ThreadSafeQueue {
public:
	template <typename Arg>
	void emplace(Arg&& args) {
		m_queue.with_lock([&](auto& queue) { queue.emplace(FWD(args)); });
		m_queue.notifyOne();
	}

	std::optional<T> pop() {
		std::optional<T> x;
		m_queue.with_lock([&](auto& queue) {
			if (!queue.empty()) {
				x.emplace(std::move(queue.front()));
				queue.pop();
			}
			});
		return x;
	}

	void waitForAnElement(std::stop_token st) {
		auto hasElement = [](const auto& queue) { return !queue.empty(); };
		m_queue.wait(hasElement, st);
	}

private:
	ConditionVariable<std::queue<T>> m_queue;
};