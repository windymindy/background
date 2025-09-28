#include "background_console_platform_windows.hpp"

#include <QtCore/QMutex>
#include <QtCore/QFileInfo>
#include <QtCore/QCoreApplication>
#include <QtCore/QtPlugin>
#include <QtCore/QLoggingCategory>
#include <QtCore/qt_windows.h>

#include "background_datatypes.hpp"

static Q_LOGGING_CATEGORY (category, "background.application")

namespace background
{

namespace
{

console_platform_windows * instance (nullptr);
QBasicMutex mutex;

extern "C" // Not required. WINAPI is already there.
{

BOOL QT_WIN_CALLBACK process_console_event (DWORD event);
LRESULT QT_WIN_CALLBACK process_user_event (HWND window, UINT event, WPARAM parameter_1, LPARAM parameter_2);

} // extern "C"

application_error failed_to_subscribe_to_events ();

} // namespace

console_platform_windows::console_platform_windows (QObject * const parent)
    : console_platform (parent)
{}

console_platform_windows::~console_platform_windows ()
{
    if (instance == nullptr)
        return;
    QMutexLocker locker (& mutex);
    instance = nullptr;
}

void console_platform_windows::start ()
{
    connect (
        this, & console_platform_windows::event_received_,
        this, & console_platform_windows::process_event,
        Qt::QueuedConnection
    );

    QMutexLocker locker (& mutex);

    if (not SetConsoleCtrlHandler (& process_console_event, TRUE))
    {
        Q_EMIT failed_to_start (failed_to_subscribe_to_events ());
        return;
    }

    const auto application (static_cast<HINSTANCE> (GetModuleHandleW (nullptr)));
    if (application == nullptr)
    {
        Q_EMIT failed_to_start (failed_to_subscribe_to_events ());
        return;
    }
    const auto name (QString::fromUtf8 (staticMetaObject.className ()));
    WNDCLASSW window;
    if (not GetClassInfoW (application, reinterpret_cast<LPCWSTR> (name.unicode ()), & window))
    {
        window =
        {
            0,
            & process_user_event,
            0, 0,
            application,
            nullptr, nullptr, nullptr, nullptr,
            reinterpret_cast<LPCWSTR> (name.unicode ())
        };
        if (not RegisterClassW (& window))
        {
            Q_EMIT failed_to_start (failed_to_subscribe_to_events ());
            return;
        }
    }
    const auto executable (QFileInfo (QCoreApplication::applicationFilePath ()).baseName ());
    const auto window_ (
        CreateWindowExW (
            0,
            window.lpszClassName,
            reinterpret_cast<LPCWSTR> (executable.unicode ()),
            0,
            0, 0, 0, 0,
            nullptr, nullptr, application, nullptr
        )
    );
    if (window_ == nullptr)
    {
        Q_EMIT failed_to_start (failed_to_subscribe_to_events ());
        return;
    }
    const QString shutdown_text;
    if (not ShutdownBlockReasonCreate (window_, reinterpret_cast<LPCWSTR> (shutdown_text.unicode ())))
    {
        qCWarning (category).noquote () << text::with_last_error (QStringLiteral (
            "Failed to subscribe to console events"
        ));
        return;
    }

    instance = this;
    locker.unlock ();

    Q_EMIT started ();
}

void console_platform_windows::stop ()
{
    {
        QMutexLocker locker (& mutex);
        instance = nullptr;
    }

    // No use cleaning up 'SetConsoleCtrlHandler ( , false)' or 'DestroyWindow ()' (and 'UnregisterClass ()').
    // The application would be terminated.

    Q_EMIT stopped ();
}

void console_platform_windows::process_event (const unsigned long event)
{
    const auto event_name = [] (const unsigned long event)
    {
        switch (event)
        {
            case CTRL_C_EVENT : return QStringLiteral ("interrupt");
            case CTRL_BREAK_EVENT : return QStringLiteral ("break");
            case CTRL_CLOSE_EVENT : return QStringLiteral ("close");
            case CTRL_LOGOFF_EVENT : return QStringLiteral ("logout");
            case CTRL_SHUTDOWN_EVENT : return QStringLiteral ("shutdown");
            default : Q_UNREACHABLE (); return QString ();
        }
    };
    Q_EMIT event_received (
        application_system_event
        { // c++20 designated initializers
            /*.action = */application_system_event::stop,
            /*.name = */event_name (event)
        }
    );
}

namespace
{

extern "C"
{

// Called in a thread spawned by the system and stopped right after the function returns.
BOOL QT_WIN_CALLBACK process_console_event (const DWORD event)
{
    switch (event)
    {
        case CTRL_C_EVENT :
        case CTRL_BREAK_EVENT :
        {
            QMutexLocker locker (& mutex);
            if (instance == nullptr)
                break;
            Q_EMIT instance->event_received_ (event);
        }
        break;

        case CTRL_CLOSE_EVENT :
        // These are never sent. These are for services.
        // By the time they are sent, there are no applications left.
        case CTRL_LOGOFF_EVENT :
        case CTRL_SHUTDOWN_EVENT :
        {
            QMutexLocker locker (& mutex);
            if (instance == nullptr)
                break;
            Q_EMIT instance->event_received_ (event);
        }
        // Returning will result in terminating the process.
        Sleep (INFINITE);
        break;

        default : break;
    }
    return TRUE;
}

// Called in the same thread to which the application object is affined.
LRESULT QT_WIN_CALLBACK process_user_event (
    const HWND window,
    const UINT event,
    const WPARAM parameter_1, const LPARAM parameter_2
)
{
    switch (event)
    {
        case WM_QUERYENDSESSION : return TRUE;

        case WM_ENDSESSION :
        if (not parameter_1 and (parameter_2 bitand ENDSESSION_CLOSEAPP) )
            return FALSE;

        if (instance == nullptr)
            return FALSE;

        if (parameter_2 bitand ENDSESSION_LOGOFF)
            Q_EMIT instance->event_received_ (CTRL_LOGOFF_EVENT);
        else
            Q_EMIT instance->event_received_ (CTRL_SHUTDOWN_EVENT);

        // After returning the process may terminate at any time.
        if (instance == nullptr)
            return FALSE;
        QCoreApplication::processEvents ();
        if (instance == nullptr)
            return FALSE;
        QCoreApplication::sendPostedEvents ();
        while (instance != nullptr)
            QCoreApplication::processEvents (QEventLoop::AllEvents bitor QEventLoop::WaitForMoreEvents);
        return FALSE;

        case WM_DESTROY : return FALSE;

        case WM_CLOSE : return FALSE;

        default : return DefWindowProcW (window, event, parameter_1, parameter_2);
    }
}

} // extern "C"

application_error failed_to_subscribe_to_events ()
{
    return application_error
    { // c++20 designated initializers
        /*.error =*/application_error::failed_to_run,
        /*.text =*/text::with_last_error (QStringLiteral (
            "Failed to run as a console application. "
            "Failed to subscribe to console events"
        ))
    };
}

} // namespace

background_console_platform_plugin_windows::background_console_platform_plugin_windows (QObject * const parent)
    : console_platform_plugin (parent)
{}

unsigned int background_console_platform_plugin_windows::order () const
{
    return 99;
}

console_platform * background_console_platform_plugin_windows::create (QObject * const parent)
{
    return new console_platform_windows (parent);
}

} // namespace background

Q_IMPORT_PLUGIN (background_console_platform_plugin_windows)
