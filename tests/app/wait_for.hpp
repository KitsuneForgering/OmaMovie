#pragma once

#include <QCoreApplication>
#include <QElapsedTimer>

#include <functional>

inline bool wait_for(const std::function<bool()>& ready) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 10000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return ready();
}
