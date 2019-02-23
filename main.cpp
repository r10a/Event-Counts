#include <iostream>
#include <stdio.h>
#include <unistd.h>

#include "EvenCount.hpp"

void waiter(EventCount *eventCount) {
    for (int i = 0; i < 100; i++) {
        std::cout << "Preparing wait: " << i << std::endl;
        auto key = eventCount->prepareWait();
        eventCount->wait(key);
        std::cout << "Got notification: " << i << std::endl;
    }
}

void notifier(EventCount *eventCount) {
    for (int i = 0; i < 100; i++) {
        std::cout << "Notifying: " << i << std::endl;
        eventCount->notify();
        std::cout << "Notified: " << i << std::endl;
        usleep(100);
    }
}

int main(int argc, char **argv) {

    EventCount eventCount;

    std::thread t1(waiter, &eventCount);
    usleep(1);
    std::thread t2(notifier, &eventCount);

    t1.join();
    t2.join();

    return 0;
}