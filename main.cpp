// 小彭老师作业05：假装是多线程 HTTP 服务器 - 富连网大厂面试官觉得很赞
#include "thread_safe_queue.h"

#include <functional>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <string>
#include <thread>
#include <map>
#include <chrono>
#include <future>


struct User {
	std::string password;
	std::string school;
	std::string phone;
};

std::map<std::string, User> users;
std::map<std::string, std::chrono::steady_clock::time_point> has_login;  // 换成 std::chrono::seconds 之类的

std::shared_mutex users_mutex;
std::shared_mutex has_login_mutex;

// 作业要求1：把这些函数变成多线程安全的
// 提示：能正确利用 shared_mutex 加分，用 lock_guard 系列加分
std::string do_register(std::string username, std::string password, std::string school, std::string phone) {	
	User user = { password, school, phone };
	std::lock_guard users_lock{ users_mutex };
	if (users.emplace(username, user).second) {
		return "注册成功";
	}
	else {
		return "用户名已被注册";
	}
}

std::string do_login(std::string username, std::string password) {
	// 作业要求2：把这个登录计时器改成基于 chrono 的
	auto now = std::chrono::steady_clock::now();   // C 语言当前时间
	{
		std::shared_lock has_login_lock_1{ has_login_mutex };
		if (has_login.find(username) != has_login.end()) {
			auto sec = now - has_login.at(username);  // C 语言算时间差
			return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(sec).count()) + "秒内登录过";
		}
	}
	{
		std::lock_guard has_login_lock_2{ has_login_mutex };
		has_login[username] = now;
	}
	std::shared_lock users_lock{ users_mutex };
	if (users.find(username) == users.end()){
		return "用户名错误";
	}
	if (users.at(username).password != password) {
		return "密码错误";
	}
	return "登录成功";
}

std::string do_queryuser(std::string username) {
	std::shared_lock users_lock{ users_mutex };
	auto& user = users.at(username);
	std::stringstream ss;
	ss << "用户名: " << username << std::endl;
	ss << "学校:" << user.school << std::endl;
	ss << "电话: " << user.phone << std::endl;
	return ss.str();
}

class ThreadPool {
public:
	explicit ThreadPool(size_t threadNum) {
		for (size_t i = 0; i < threadNum; ++i) {
			_threads.emplace_back([this](std::stop_token st) {
				while (!st.stop_requested()) {
					_tasks.waitForAnElement(st);
					auto task = _tasks.pop();
					if (task.has_value()) {
						task.value()();
					}
				}
				});
		}
	}

	//~ThreadPool() {
	//	//_cv.notify_all();
	//	for (auto& thread : _threads) {
	//		if (thread.joinable()) {
	//			thread.request_stop();
	//			thread.join();
	//		}
	//	}
	//}

	template<typename F, typename... Args>
	auto create(F&& f, Args&&... args) {
		auto taskPtr = std::make_shared<std::packaged_task<std::invoke_result_t<F, Args...>()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)
			);

		_tasks.emplace([taskPtr]() { (*taskPtr)(); });

		return taskPtr->get_future();
	}

private:
	std::vector<std::jthread> _threads;
	ThreadSafeQueue<std::function<void()>> _tasks;
	//std::mutex _mtx;
	//std::condition_variable _cv;
};

//struct ThreadPool {
//	void create(std::function<void()> start) {
//		// 作业要求3：如何让这个线程保持在后台执行不要退出？
//		// 提示：改成 async 和 future 且用法正确也可以加分
//		std::thread thr(start);
//	}
//};


namespace test {  // 测试用例？出水用力！
	std::string username[] = { "张心欣", "王鑫磊", "彭于斌", "胡原名" };
	std::string password[] = { "hellojob", "anti-job42", "cihou233", "reCihou_!" };
	std::string school[] = { "九百八十五大鞋", "浙江大鞋", "剑桥大鞋", "麻绳理工鞋院" };
	std::string phone[] = { "110", "119", "120", "12315" };
}

int main() {
	ThreadPool tpool(8);
	std::vector<std::future<std::string>> results;
	for (int i = 0; i < 262144; i++) {
		results.emplace_back(tpool.create([&] {
			return do_register(test::username[rand() % 4], test::password[rand() % 4], test::school[rand() % 4], test::phone[rand() % 4]);
			}));
		results.emplace_back(tpool.create([&] {
			return do_login(test::username[rand() % 4], test::password[rand() % 4]);
			}));
		results.emplace_back(tpool.create([&] {
			return do_queryuser(test::username[rand() % 4]);
			}));
	}

	for (auto& result : results) {
		std::cout << result.get() << std::endl;
	}

	// 作业要求4：等待 tpool 中所有线程都结束后再退出
	return 0;
}
