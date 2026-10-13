#pragma once

#include <QCoreApplication>
#include <QElapsedTimer>

#include <functional>

inline bool wait_for(const std::function<bool()>& ready, int timeout_ms = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < timeout_ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return ready();
}
