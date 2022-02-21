#include <boost/asio.hpp>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <utility>
#include "bank.h"

using boost::asio::ip::tcp;

void client_(bank::ledger &ledger, tcp::socket socket, std::mutex &cout_mutex) {
    tcp::iostream client(std::move(socket));
    client << "What is your name?" << std::endl;
    if (!client) {
        cout_mutex.lock();
        std::cout << "Disonnected " << client.socket().remote_endpoint()
                  << " --> " << client.socket().local_endpoint() << std::endl;
        cout_mutex.unlock();
        return;
    }
    std::string name;
    client >> name;
    bank::user &current_user = ledger.get_or_create_user(name);
    client << "Hi " << name << std::endl;
    if (!client) {
        cout_mutex.lock();
        std::cout << "Disonnected " << client.socket().remote_endpoint()
                  << " --> " << client.socket().local_endpoint() << std::endl;
        cout_mutex.unlock();
        return;
    }
    std::string command;
    while (client >> command) {
        if (command == "balance") {
            client << current_user.balance_xts() << std::endl;
            if (!client) {
                cout_mutex.lock();
                std::cout << "Disonnected " << client.socket().remote_endpoint()
                          << " --> " << client.socket().local_endpoint()
                          << std::endl;
                cout_mutex.unlock();
                return;
            }
        } else if (command == "transfer") {
            std::string counterparty_name;
            int amount = 0;
            std::string comment;
            client >> counterparty_name;
            client >> amount;
            client.ignore();
            getline(client, comment);
            try {
                current_user.transfer(
                    ledger.get_or_create_user(counterparty_name), amount,
                    comment);
                client << "OK" << std::endl;
                if (!client) {
                    cout_mutex.lock();
                    std::cout << "Disonnected "
                              << client.socket().remote_endpoint() << " --> "
                              << client.socket().local_endpoint() << std::endl;
                    cout_mutex.unlock();
                    return;
                }
            } catch (bank::transfer_error &e) {
                client << e.what() << std::endl;
            }
        } else if (command == "transactions" || command == "monitor") {
            int quantity = 0;
            client >> quantity;
            if (!client) {
                cout_mutex.lock();
                std::cout << "Disonnected " << client.socket().remote_endpoint()
                          << " --> " << client.socket().local_endpoint()
                          << std::endl;
                cout_mutex.unlock();
                return;
            }
            client << "CPTY\tBAL\tCOMM" << std::endl;
            if (!client) {
                cout_mutex.lock();
                std::cout << "Disonnected " << client.socket().remote_endpoint()
                          << " --> " << client.socket().local_endpoint()
                          << std::endl;
                cout_mutex.unlock();
                return;
            }
            std::vector<bank::transaction> transactions_snapshot;
            int cur_balance = 0;
            auto iterator = current_user.snapshot_transactions(
                [&](const auto &transactions, int balance) {
                    if (transactions.size() <=
                        static_cast<unsigned int>(quantity)) {
                        transactions_snapshot = std::vector(
                            transactions.begin(), transactions.end());
                    } else {
                        transactions_snapshot = std::vector(
                            transactions.end() - quantity, transactions.end());
                    }
                    cur_balance = balance;
                });
            for (auto &transaction : transactions_snapshot) {
                if (transaction.counterparty == nullptr) {
                    client << "-\t";
                    client.flush();
                    if (!client) {
                        cout_mutex.lock();
                        std::cout << "Disonnected "
                                  << client.socket().remote_endpoint()
                                  << " --> " << client.socket().local_endpoint()
                                  << std::endl;
                        cout_mutex.unlock();
                        return;
                    }
                } else {
                    client << transaction.counterparty->name() << "\t";
                    client.flush();
                    if (!client) {
                        cout_mutex.lock();
                        std::cout << "Disonnected "
                                  << client.socket().remote_endpoint()
                                  << " --> " << client.socket().local_endpoint()
                                  << std::endl;
                        cout_mutex.unlock();
                        return;
                    }
                }
                client << transaction.balance_delta_xts << "\t"
                       << transaction.comment << std::endl;
            }
            client << "===== BALANCE: " << cur_balance
                   << " XTS =====" << std::endl;
            if (!client) {
                cout_mutex.lock();
                std::cout << "Disonnected " << client.socket().remote_endpoint()
                          << " --> " << client.socket().local_endpoint()
                          << std::endl;
                cout_mutex.unlock();
                return;
            }
            if (command == "monitor") {
                while (true) {
                    bank::transaction new_transaction =
                        iterator.wait_next_transaction();
                    client << new_transaction.counterparty->name() << "\t"
                           << new_transaction.balance_delta_xts << "\t"
                           << new_transaction.comment << std::endl;
                }
            }
        } else {
            client << "Unknown command: '" << command << "'" << std::endl;
            if (!client) {
                cout_mutex.lock();
                std::cout << "Disonnected " << client.socket().remote_endpoint()
                          << " --> " << client.socket().local_endpoint()
                          << std::endl;
                cout_mutex.unlock();
                return;
            }
        }
    }
    cout_mutex.lock();
    std::cout << "Disonnected " << client.socket().remote_endpoint() << " --> "
              << client.socket().local_endpoint() << std::endl;
    cout_mutex.unlock();
}
int main([[maybe_unused]] int argc, char *argv[]) {
    try {
        assert(argc == 3);
        boost::asio::io_context io_context;
        int port = std::atoi(argv[1]);  // NOLINT
        tcp::acceptor acceptor(
            io_context,
            tcp::endpoint(tcp::v4(), static_cast<unsigned short>(port)));
        std::cout << "Listening at " << acceptor.local_endpoint() << std::endl;
        std::string filename(argv[2]);  // NOLINT
        std::ofstream os(filename);
        if (!os) {
            std::cerr << "Unable to store port to file " << filename
                      << std::endl;
        }
        os << acceptor.local_endpoint().port();
        if (os.fail()) {
            std::cerr << "Unable to store port to file " << filename
                      << std::endl;
        }
        os.close();
        std::mutex cout_mutex;
        bank::ledger ledger;
        while (true) {
            tcp::socket s = acceptor.accept();
            cout_mutex.lock();
            std::cout << "Connected " << s.remote_endpoint() << " --> "
                      << s.local_endpoint() << std::endl;
            cout_mutex.unlock();
            std::thread t(client_, std::ref(ledger), std::move(s),
                          std::ref(cout_mutex));
            t.detach();
        }
    } catch (...) {
    }
}