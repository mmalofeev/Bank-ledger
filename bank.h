#ifndef BANK_H_
#define BANK_H_
#include <condition_variable>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
namespace bank {
// IgnorePublicMemberVariables
struct transaction;
struct user;
struct user_transactions_iterator {
    transaction wait_next_transaction();
    explicit user_transactions_iterator(const user *user__);

private:
    const user *user_;
    std::size_t index_;
};
struct user {
private:
    std::string name_;
    int balance_xts_;
    mutable std::condition_variable_any transactions_updated_cv;
    mutable std::shared_mutex m;
    std::vector<transaction> transactions{};

public:
    friend struct ledger;
    friend struct user_transactions_iterator;
    explicit user(std::string name);
    const std::string &name() const noexcept;
    int balance_xts() const;
    [[nodiscard]] user_transactions_iterator monitor() const &;
    template <typename Func>
    user_transactions_iterator snapshot_transactions(Func func) const & {
        std::shared_lock l{m};
        func(transactions, balance_xts_);
        return user_transactions_iterator(this);
    }

    void transfer(user &counterparty,
                  int amount_of_xts,
                  const std::string &comment);
};
struct transaction {
    // NOLINT
    const user *const counterparty;  // NOLINT
    const int balance_delta_xts;     // NOLINT
    const std::string comment;       // NOLINT
    transaction(const user *counterparty_,
                int balance_delta_xts_,
                std::string comment_);
};
struct ledger {
private:
    std::unordered_map<std::string, std::unique_ptr<user>> users;
    mutable std::mutex m;

public:
    user &get_or_create_user(const std::string &name) &;
};

struct transfer_error : std::logic_error {
    explicit transfer_error(const std::string &s);
};

struct not_enough_funds_error : transfer_error {
    explicit not_enough_funds_error(int real_amount, int requested_amount);
};

struct negative_amount_of_funds : transfer_error {
    explicit negative_amount_of_funds(int amount);
};

struct transfer_money_to_himself : transfer_error {
    explicit transfer_money_to_himself();
};

}  // namespace bank
#endif