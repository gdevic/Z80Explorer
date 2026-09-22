#ifndef CLASSMCPTHREADING_H
#define CLASSMCPTHREADING_H

#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>
#include <functional>
#include <type_traits>

/*
 * ClassMcpThreading — main-thread marshalling guard.
 *
 * Sim state and the Qt graphics stack may only be touched from the main
 * (GUI) thread, where the controller lives.
 *
 * QHttpServer dispatches its route handlers on the thread it was created
 * on, and ClassMcpServer is created on the main thread, so MCP handlers
 * already run there and callOnMain() invokes the callable directly. The
 * queued branch keeps the invariant true if the server is ever moved to a
 * worker thread; it is not a claim that handlers arrive from one today.
 * Works with void and non-void return types.
 *
 * Note what the direct path implies: a handler that blocks, or that spins
 * a nested event loop, does so on the GUI thread. A long tool must bound
 * its wait, and ClassMcpServer serialises the tools that cannot tolerate
 * being re-entered from inside such a loop.
 */
namespace ClassMcpThreading
{
    // Returns true if the current thread is the Qt main (GUI) thread
    inline bool onMainThread()
    {
        QCoreApplication *app = QCoreApplication::instance();
        return !app || QThread::currentThread() == app->thread();
    }

    // Invoke `fn` on the main thread, blocking until it completes. Returns fn()'s result.
    template <typename Func>
    auto callOnMain(Func &&fn) -> std::invoke_result_t<Func>
    {
        using Ret = std::invoke_result_t<Func>;
        if (onMainThread())
            return fn();

        QCoreApplication *app = QCoreApplication::instance();
        if constexpr (std::is_void_v<Ret>)
        {
            QMetaObject::invokeMethod(app, std::forward<Func>(fn), Qt::BlockingQueuedConnection);
        }
        else
        {
            Ret result{};
            QMetaObject::invokeMethod(app,
                                      [&result, &fn]() { result = fn(); },
                                      Qt::BlockingQueuedConnection);
            return result;
        }
    }
}

#endif // CLASSMCPTHREADING_H
