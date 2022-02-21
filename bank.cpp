#include "bank.h"
#include <memory>

namespace {
const int start_balance = 100;
}  // namespace

namespace bank {
user::user(std::string name)
    : name_(std::move(name)), balance_xts_(start_balance) {
}
const std::string &user::name() const noexcept {
    return name_;
}
int user::balance_xts() const {
    std::shared_lock l{m};
    return balance_xts_;
}

transaction::transaction(const user *counterparty_,
                         int balance_delta_xts_,
                         std::string comment_)
    : counterparty(counterparty_),
      balance_delta_xts(balance_delta_xts_),
      comment(std::move(comment_)) {
}

void user::transfer(user &counterparty,
                    int amount_of_xts,
                    const std::string &comment) {
    if (&counterparty == this) {
        throw transfer_money_to_himself();
    }
    if (amount_of_xts <= 0) {
        throw negative_amount_of_funds(amount_of_xts);
    }
    std::scoped_lock l{m, counterparty.m};
    if (amount_of_xts > balance_xts_) {
        throw not_enough_funds_error(balance_xts_, amount_of_xts);
    }
    balance_xts_ -= amount_of_xts;
    counterparty.balance_xts_ += amount_of_xts;
    counterparty.transactions.emplace_back(this, amount_of_xts, comment);
    transactions.emplace_back(&counterparty, -amount_of_xts, comment);
    transactions_updated_cv.notify_all();
    counterparty.transactions_updated_cv.notify_all();
}

user &ledger::get_or_create_user(const std::string &name) & {
    std::unique_lock l{m};
    if (users[name] == nullptr) {
        users[name] = std::make_unique<user>(name);
        users[name]->transactions.emplace_back(nullptr, start_balance,
                                               "Initial deposit for " + name);
    }
    return *users[name];
}

transfer_error::transfer_error(const std::string &s) : std::logic_error(s) {
}

not_enough_funds_error::not_enough_funds_error(int real_amount,
                                               int requested_amount)
    : transfer_error("Not enough funds: " + std::to_string(real_amount) +
                     " XTS available, " + std::to_string(requested_amount) +
                     " XTS requested") {
}

user_transactions_iterator::user_transactions_iterator(const user *user__)
    : user_(user__), index_(user__->transactions.size()) {
}
transaction user_transactions_iterator::wait_next_transaction() {
    std::shared_lock l(user_->m);
    user_->transactions_updated_cv.wait(
        l, [&]() { return user_->transactions.size() > index_; });
    return user_->transactions[index_++];
}

user_transactions_iterator user::monitor() const & {
    return snapshot_transactions([](const std::vector<transaction> &, int) {});
}

negative_amount_of_funds::negative_amount_of_funds(int amount)
    : transfer_error("Expected positive amount of XTS, got " +
                     std::to_string(amount)) {
}

transfer_money_to_himself::transfer_money_to_himself()
    : transfer_error(
          "Trying to transfer money to himself, expected other user") {
}

}  // namespace bank