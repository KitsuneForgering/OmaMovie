#pragma once

// The one way work reaches the UI thread from another thread (CLAUDE.md §13): a queued call on
// `receiver`. Qt's event queue orders the hand-off, but a distribution's Qt is not built with
// ThreadSanitizer, so that ordering is invisible to TSan and every capture would be reported as
// a race, drowning real ones (M10 application sanitizer subset). The release before posting and
// the acquire before running tell TSan what the queue already guarantees. All hand-offs share
// one address, as Qt funnels them through the UI thread's one event queue.

#include <QMetaObject>
#include <QObject>

#include <utility>

#if defined(__SANITIZE_THREAD__)
#include <sanitizer/tsan_interface.h> // __tsan_acquire, __tsan_release (GCC and Clang)
// Exported by the TSan runtime but declared by neither toolchain's header.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
#endif
extern "C" void __tsan_ignore_thread_begin();
extern "C" void __tsan_ignore_thread_end();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#endif

namespace ui_post_detail {
inline char hand_off;
inline void release() {
#if defined(__SANITIZE_THREAD__)
    __tsan_release(&hand_off);
#endif
}
// Qt allocates and fills the call's storage inside invokeMethod, after release(): TSan would
// record those writes as unordered with the UI thread's reads, so they are not recorded.
inline void ignore(bool on) {
#if defined(__SANITIZE_THREAD__)
    if (on)
        __tsan_ignore_thread_begin();
    else
        __tsan_ignore_thread_end();
#else
    (void)on;
#endif
}
inline void acquire() {
#if defined(__SANITIZE_THREAD__)
    __tsan_acquire(&hand_off);
#endif
}
} // namespace ui_post_detail

template <typename Work>
void post_to_ui(QObject* receiver, Work&& work) {
    ui_post_detail::release();
    ui_post_detail::ignore(true);
    QMetaObject::invokeMethod(
        receiver,
        [work = std::forward<Work>(work)]() mutable {
            ui_post_detail::acquire(); // before the captures are read
            work();
        },
        Qt::QueuedConnection);
    ui_post_detail::ignore(false);
}
