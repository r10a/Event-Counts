#pragma once

#include <atomic>
#include <climits>
#include <thread>

#include "Futex.hpp"
#include "Likely.hpp"

constexpr auto kIsLittleEndian = true;

class EventCount {
public:
    EventCount() noexcept : val_(0) {}

    class Key {
        friend class EventCount;

        explicit Key(uint32_t e) noexcept : epoch_(e) {}

        uint32_t epoch_;
    };

    void notify() noexcept;

    void notifyAll() noexcept;

    Key prepareWait() noexcept;

    void cancelWait() noexcept;

    void wait(Key key) noexcept;

    /**
     * Wait for condition() to become true.  Will clean up appropriately if
     * condition() throws, and then rethrow.
     */
    template<class Condition>
    void await(Condition condition);

private:
    void doNotify(int n) noexcept;

    EventCount(const EventCount &) = delete;

    EventCount(EventCount &&) = delete;

    EventCount &operator=(const EventCount &) = delete;

    EventCount &operator=(EventCount &&) = delete;

    // This requires 64-bit
    static_assert(sizeof(int) == 4, "bad platform");
    static_assert(sizeof(uint32_t) == 4, "bad platform");
    static_assert(sizeof(uint64_t) == 8, "bad platform");
    static_assert(sizeof(std::atomic<uint64_t>) == 8, "bad platform");
    static_assert(sizeof(Futex<std::atomic>) == 4, "bad platform");

    static constexpr size_t kEpochOffset = kIsLittleEndian ? 1 : 0;

    // val_ stores the epoch in the most significant 32 bits and the
    // waiter count in the least significant 32 bits.
    std::atomic<uint64_t> val_;

    static constexpr uint64_t kAddWaiter = uint64_t(1);
    static constexpr uint64_t kSubWaiter = uint64_t(-1);
    static constexpr size_t kEpochShift = 32;
    static constexpr uint64_t kAddEpoch = uint64_t(1) << kEpochShift;
    static constexpr uint64_t kWaiterMask = kAddEpoch - 1;
};

inline void EventCount::notify() noexcept {
    doNotify(1);
}

inline void EventCount::notifyAll() noexcept {
    doNotify(INT_MAX);
}

inline void EventCount::doNotify(int n) noexcept {
    uint64_t prev = val_.fetch_add(kAddEpoch, std::memory_order_acq_rel);
    if (UNLIKELY(prev & kWaiterMask)) {
        futexWake(reinterpret_cast<Futex<std::atomic> *>(&val_) + kEpochOffset, n);
    }
}

inline EventCount::Key EventCount::prepareWait() noexcept {
    uint64_t prev = val_.fetch_add(kAddWaiter, std::memory_order_acq_rel);
    return Key(prev >> kEpochShift);
}

inline void EventCount::cancelWait() noexcept {
    // memory_order_relaxed would suffice for correctness, but the faster
    // #waiters gets to 0, the less likely it is that we'll do spurious wakeups
    // (and thus system calls).
//    uint64_t prev = val_.fetch_add(kSubWaiter, std::memory_order_seq_cst);
//    DCHECK_NE((prev & kWaiterMask), 0);
    val_.fetch_add(kSubWaiter, std::memory_order_seq_cst);
}

inline void EventCount::wait(Key key) noexcept {
    while ((val_.load(std::memory_order_acquire) >> kEpochShift) == key.epoch_) {

        futexWait(
                reinterpret_cast<Futex<std::atomic> *>(&val_) + kEpochOffset,
                key.epoch_);
    }
    // memory_order_relaxed would suffice for correctness, but the faster
    // #waiters gets to 0, the less likely it is that we'll do spurious wakeups
    // (and thus system calls)
//    uint64_t prev = val_.fetch_add(kSubWaiter, std::memory_order_seq_cst);
//    DCHECK_NE((prev & kWaiterMask), 0);
    val_.fetch_add(kSubWaiter, std::memory_order_seq_cst);
}

template<class Condition>
void EventCount::await(Condition condition) {
    if (condition()) {
        return; // fast path
    }

    // condition() is the only thing that may throw, everything else is
    // noexcept, so we can hoist the try/catch block outside of the loop
    try {
        for (;;) {
            auto key = prepareWait();
            if (condition()) {
                cancelWait();
                break;
            } else {
                wait(key);
            }
        }
    } catch (...) {
        cancelWait();
        throw;
    }
}
