#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <random>
#include <unordered_map>
#include <cstring>

#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/split.hpp>


using namespace boost;


class DataBase {
public:
    void Update(size_t bid, size_t offer) {
        offer_ = offer;
        bid_ = bid;
    }

    size_t GetBid() const { return bid_; }
    size_t GetOffer() const { return offer_; }


    static DataBase* GetBase() {
        if (instance_ == nullptr) {
            instance_ = new DataBase(1,1);
        }
        return instance_;
    }

private:
    static DataBase* instance_;
    DataBase(size_t bid, size_t offer)
            : bid_(bid), offer_(offer) {
    }
    size_t bid_;
    size_t offer_;
};

DataBase* DataBase::instance_ = nullptr;


class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(asio::ip::tcp::socket socket);
    void start();
    void Push(size_t bid, size_t offer);

private:
    void do_read();

    void do_write(std::size_t length);
    asio::ip::tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length]{'\0'};
    char ans_buff_[max_length]{'\0'};
};


class FollowPool {
public:
    size_t Follow(std::shared_ptr<Session> session) {
        followers_[pk_++] = session;
        return pk_ - 1;
    }

    void Push() {
        std::vector<size_t> keys;
        for (auto&& [key, weak_ptr] : followers_) {
            //std::cout << key << std::endl;
            if (weak_ptr.expired()) {
                keys.push_back(key);
            } else {
                std::shared_ptr<Session> ptr = weak_ptr.lock();
                auto* base = DataBase::GetBase();
                ptr->Push(base->GetBid(), base->GetOffer());
            }
        }
        for (auto key : keys) {
            UnFollow(key);
        }
    }

    void UnFollow(size_t pk) {
        followers_.erase(pk);
    }

    static FollowPool* GetFollowManager() {
        if (pool_ == nullptr) {
            pool_ = new FollowPool;
        }
        return pool_;
    }

private:
    FollowPool() = default;
    static FollowPool* pool_;
    size_t pk_{0};
    std::unordered_map<size_t, std::weak_ptr<Session>> followers_;
};

FollowPool* FollowPool::pool_{nullptr};

Session::Session(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

void Session::start() {
    {
        std::shared_ptr<Session> self(shared_from_this());
        FollowPool::GetFollowManager()->Follow(self);
    }
    do_read();
}

void Session::do_read() {
    std::shared_ptr<Session> self(shared_from_this());

    socket_.async_read_some(asio::buffer(data_, max_length),[this, self] (std::error_code ec, std::size_t length){
        if (!ec) {
            std::string data(data_);
            memset(data_, '\0', max_length);
            std::vector<std::string> v;
            boost::algorithm::split(v, data, [](auto t) {return t == ' '; });
            if (v.size() < 2) {
                do_read();
                return;
            }
            if (v[0] == "buy" && v.size() == 2) {
                size_t offer = std::stoull(v[1]);
                std::string ans = "res Reject\n";
                if (DataBase::GetBase()->GetOffer()  == offer) {
                    ans = "res Done @  " + std::to_string(offer) + "\n";
                }
                memset(ans_buff_, '\0', max_length);
                for (size_t i = 0; i < ans.length(); i++) {
                    ans_buff_[i] = ans[i];
                }
                do_write(ans.length());

            } else if (v[0] == "sell" && v.size() == 2) {
                size_t bid = std::stoull(v[1]);
                std::string ans = "res Reject\n";
                if (DataBase::GetBase()->GetBid()  == bid) {
                    ans = "res Done @ " + std::to_string(bid) + "\n";
                }
                memset(ans_buff_, '\0', max_length);
                for (size_t i = 0; i < ans.length(); i++) {
                    ans_buff_[i] = ans[i];
                }
                do_write(ans.length());
            }
            do_read();
        }
    });
}

void Session::do_write(std::size_t length) {
    std::shared_ptr<Session> self(shared_from_this());
    asio::async_write(socket_, asio::buffer(ans_buff_, length),[this, self] (std::error_code ec, std::size_t /*length*/){
        if (!ec) {
            do_read();
        }
    });
}

void Session::Push(size_t bid, size_t offer) {
    std::shared_ptr<Session> self(shared_from_this());
    std::string info = "upd " + std::to_string(bid) + " " + std::to_string(offer) + "\n";
    //std::cout << info << std::endl;
    asio::async_write(socket_, asio::buffer(info, info.length()),[this, self] (std::error_code ec, std::size_t /*length*/){
    });
}


class Server {
public:
    Server(asio::io_context& io_context, short port) :
            acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
            socket_(io_context) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(socket_, [this](std::error_code ec) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket_))->start();
            }
            do_accept();
        });
    }

    asio::ip::tcp::acceptor acceptor_;
    asio::ip::tcp::socket socket_;
};


class PriceUpdater : public std::enable_shared_from_this<PriceUpdater> {
public:
    PriceUpdater(asio::io_context& ctx, int timeout)
            : timer_(ctx, posix_time::microseconds(timeout)),
              timeout_(timeout),
              ctx_(ctx) {
    }

    void Run() {
        std::shared_ptr<PriceUpdater> self(shared_from_this());
        timer_.async_wait([self](const system::error_code& error) {
            std::random_device random;

            std::uniform_int_distribution<size_t> randomDistribution(1, 1000);
            size_t bid = randomDistribution(random);
            size_t offer = bid + randomDistribution(random);
            DataBase::GetBase()->Update(bid, offer);
            FollowPool::GetFollowManager()->Push();
            //std::cout << "bid: " << bid << " offer: " << offer << std::endl;
            std::make_shared<PriceUpdater>(self->ctx_, self->timeout_)->Run();
        });
    }
private:
    asio::deadline_timer timer_;
    asio::io_context& ctx_;
    int timeout_;
};

int main() {
    asio::io_context io_context;
    Server server(io_context, 8080);
    std::make_shared<PriceUpdater>(io_context, 3000)->Run();
    io_context.run();
    return 0;
}
