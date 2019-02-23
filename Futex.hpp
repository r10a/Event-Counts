#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <limits>
#include <type_traits>

enum class FutexResult {
    VALUE_CHANGED, /* futex value didn't match expected */
    AWOKEN, /* wakeup by matching futex wake, or spurious wakeup */
    INTERRUPTED, /* wakeup by interrupting signal */
    TIMEDOUT, /* wakeup by expiring deadline */
};

/**
 * Futex is an atomic 32 bit unsigned integer that provides access to the
 * futex() syscall on that value.  It is templated in such a way that it
 * can interact properly with DeterministicSchedule testing.
 *
 * If you don't know how to use futex(), you probably shouldn't be using
 * this class.  Even if you do know how, you should have a good reason
 * (and benchmarks to back you up).
 *
 * Because of the semantics of the futex syscall, the futex family of
 * functions are available as free functions rather than member functions
 */
template<template<typename> class Atom = std::atomic>
using Futex = Atom<std::uint32_t>;

/**
 * Puts the thread to sleep if this->load() == expected.  Returns true when
 * it is returning because it has consumed a wake() event, false for any
 * other return (signal, this->load() != expected, or spurious wakeup).
 */
template<typename Futex>
FutexResult futexWait(const Futex *futex, uint32_t expected, uint32_t waitMask = -1);


/**
 * Similar to futexWait but also accepts a deadline until when the wait call
 * may block.
 *
 * Optimal clock types: std::chrono::system_clock, std::chrono::steady_clock.
 * NOTE: On some systems steady_clock is just an alias for system_clock,
 * and is not actually steady.
 *
 * For any other clock type, now() will be invoked twice.
 */
template<
        typename Futex,
        class Clock,
        class Duration = typename Clock::duration>
FutexResult futexWaitUntil(
        const Futex *futex,
        uint32_t expected,
        std::chrono::time_point<Clock, Duration> const &deadline,
        uint32_t waitMask = -1);

/**
 * Wakes up to count waiters where (waitMask & wakeMask) != 0, returning the
 * number of awoken threads, or -1 if an error occurred.  Note that when
 * constructing a concurrency primitive that can guard its own destruction, it
 * is likely that you will want to ignore EINVAL here (as well as making sure
 * that you never touch the object after performing the memory store that is
 * the linearization point for unlock or control handoff).  See
 * https://sourceware.org/bugzilla/show_bug.cgi?id=13690
 */
template<typename Futex>
int futexWake(
        const Futex *futex,
        int count = std::numeric_limits<int>::max(),
        uint32_t wakeMask = -1);

/** Optimal when TargetClock is the same type as Clock.
 *
 *  Otherwise, both Clock::now() and TargetClock::now() must be invoked. */
template<typename TargetClock, typename Clock, typename Duration>
typename TargetClock::time_point time_point_conv(
        std::chrono::time_point<Clock, Duration> const &time) {
    using std::chrono::duration_cast;
    using TimePoint = std::chrono::time_point<Clock, Duration>;
    using TargetDuration = typename TargetClock::duration;
    using TargetTimePoint = typename TargetClock::time_point;
    if (time == TimePoint::max()) {
        return TargetTimePoint::max();
    } else if (std::is_same<Clock, TargetClock>::value) {
        // in place of time_point_cast, which cannot compile without if-constexpr
        auto const delta = time.time_since_epoch();
        return TargetTimePoint(duration_cast<TargetDuration>(delta));
    } else {
        // different clocks with different epochs, so non-optimal case
        auto const delta = time - Clock::now();
        return TargetClock::now() + duration_cast<TargetDuration>(delta);
    }
}

/**
 * Available overloads, with definitions elsewhere
 *
 * These functions are treated as ADL-extension points, the templates above
 * call these functions without them having being pre-declared.  This works
 * because ADL lookup finds the definitions of these functions when you pass
 * the relevant arguments
 */
int futexWakeImpl(
        const Futex<std::atomic> *futex,
        int count,
        uint32_t wakeMask);

FutexResult futexWaitImpl(
        const Futex<std::atomic> *futex,
        uint32_t expected,
        std::chrono::system_clock::time_point const *absSystemTime,
        std::chrono::steady_clock::time_point const *absSteadyTime,
        uint32_t waitMask);

template<typename Futex, typename Deadline>
typename std::enable_if<Deadline::clock::is_steady, FutexResult>::type
futexWaitImpl(
        Futex *futex,
        uint32_t expected,
        Deadline const &deadline,
        uint32_t waitMask) {
    return futexWaitImpl(futex, expected, nullptr, &deadline, waitMask);
}

template<typename Futex, typename Deadline>
typename std::enable_if<!Deadline::clock::is_steady, FutexResult>::type
futexWaitImpl(
        Futex *futex,
        uint32_t expected,
        Deadline const &deadline,
        uint32_t waitMask) {
    return futexWaitImpl(futex, expected, &deadline, nullptr, waitMask);
}

template<typename Futex>
FutexResult
futexWait(const Futex *futex, uint32_t expected, uint32_t waitMask) {
    auto rv = futexWaitImpl(futex, expected, nullptr, nullptr, waitMask);
    assert(rv != FutexResult::TIMEDOUT);
    return rv;
}

template<typename Futex>
int futexWake(const Futex *futex, int count, uint32_t wakeMask) {
    return futexWakeImpl(futex, count, wakeMask);
}

template<typename Futex, class Clock, class Duration>
FutexResult futexWaitUntil(
        const Futex *futex,
        uint32_t expected,
        std::chrono::time_point<Clock, Duration> const &deadline,
        uint32_t waitMask) {
    using Target = typename std::conditional<
            Clock::is_steady,
            std::chrono::steady_clock,
            std::chrono::system_clock>::type;
    auto const converted = time_point_conv<Target>(deadline);
    return converted == Target::time_point::max()
           ? futexWaitImpl(futex, expected, nullptr, nullptr, waitMask)
           : futexWaitImpl(futex, expected, converted, waitMask);
}