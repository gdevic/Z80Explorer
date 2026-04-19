#ifndef CLASSMCPTHREADING_H
#define CLASSMCPTHREADING_H

#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>
#include <functional>
#include <type_traits>

/*
 * ClassMcpThreading — cross-thread marshalling helper.
 *
 * MCP tool handlers arrive on QHttpServer's handler thread (or any thread
 * the Qt dispatcher uses). Sim state and the Qt graphics stack must only
 * be touched from the main (GUI) thread, where the controller lives.
 *
 * callOnMain() runs the given callable synchronously on the main thread
 * via Qt::BlockingQueuedConnection, returning its result to the caller.
 * If already on the main thread, the callable is invoked directly with
 * no queue round-trip. Works with void and non-void return types.
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
