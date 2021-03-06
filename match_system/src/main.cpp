// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include "gen-cpp/Match.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

// add
#include <iostream>
#include <thread> // 加线程
#include <mutex> // 加锁
#include <condition_variable> // 条件变量
#include <queue> // 消息队列


using namespace std;

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace  ::match_service;

struct Task{
    User user;
    string type;
};

struct MessageQueue{
    queue<Task> q;
    mutex m;

    condition_variable cv; // 条件变量对锁进行了封装
} message_queue;

class Poll{
    public:
        void add(User user) {
            users.push_back(user);
        }
        void remove(User user) {
            for (size_t i = 0; i < users.size(); i++) {
                if (users[i].id == user.id) {
                    users.erase(users.begin() + i);
                    break; // 只删除一个
                }
            }
        }

        void match() {
            while (users.size() > 1) {
                auto a = users[0];
                auto b = users[1];
                users.erase(users.begin());
                users.erase(users.begin());

                save_result(a.id, b.id);
            }
        }

        void save_result(int a, int b) {
            // 用来存储匹配之后的结果
            printf("Match result: %d %d \n", a, b);
        }
    private:
        vector<User> users;
} pool;


class MatchHandler : virtual public MatchIf {
    public:
        MatchHandler() {
            // Your initialization goes here
        }

        int32_t add_user(const User& user, const std::string& Info) {
            // Your implementation goes here
            printf("add_user\n");

            // 加锁
            unique_lock<mutex> lck(message_queue.m);

            // 加到队列中
            message_queue.q.push({user, "add"});

            // 唤醒条件变量
            message_queue.cv.notify_all();

            return 0;
        }

        int32_t remove_user(const User& user, const std::string& Info) {
            // Your implementation goes here
            printf("remove_user\n");

            // 同样上锁
            unique_lock<mutex> lck(message_queue.m);
            message_queue.q.push({user, "remove"});

            // 有操作之后唤醒条件变量
            message_queue.cv.notify_all();


            return 0;
        }
};

void consume_task() {
    while (true) {
        unique_lock<mutex> lck(message_queue.m);
        // 同样加锁
        if (message_queue.q.empty()) {
            message_queue.cv.wait(lck);
            // 如果队列为空，线程应该卡死
            // 直到有新的玩家进来再继续 cv.wait 条件变量等待通知
        } else {
            auto task = message_queue.q.front();
            message_queue.q.pop();

            lck.unlock();

            // 先解锁，然后再进行操作，防止锁占用时间过长导致 add remove 不可以使用
            if (task.type == "add") pool.add(task.user);
            else if (task.type == "remove") pool.remove(task.user);

            pool.match();
        }
    }
}

int main(int argc, char **argv) {
    int port = 9090;
    ::std::shared_ptr<MatchHandler> handler(new MatchHandler());
    ::std::shared_ptr<TProcessor> processor(new MatchProcessor(handler));
    ::std::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
    ::std::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    ::std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

    TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);

    cout << "strat match server" << endl;

    thread matching_thread(consume_task);

    server.serve();
    return 0;
}

