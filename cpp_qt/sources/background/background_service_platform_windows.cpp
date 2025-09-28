#include "background_service_platform_windows.hpp"

#include <QtCore/QThread>
#include <QtCore/QMutex>
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

enum struct starting_sequence
{
    none,
    checkpoint_1,
    checkpoint_2,
    done
};

enum struct stopping_sequence
{
    none,
    stop,
    done
};

struct run_service_result_
{
    std::optional<application_error> value;

    bool failed () const;
    application_error error ();
};

service_platform_windows * instance (nullptr);
QBasicMutex mutex;
starting_sequence starting (starting_sequence::none);
stopping_sequence stopping (stopping_sequence::none);
Q_GLOBAL_STATIC (run_service_result_, run_service_result);
SERVICE_STATUS_HANDLE service (nullptr);
SERVICE_STATUS state { 0, 0, 0, 0, 0, 0, 0 };

void run_service ();

extern "C" // Not required. WINAPI is already there.
{

void QT_WIN_CALLBACK run_service_ (DWORD argc, LPWSTR * argv);
DWORD QT_WIN_CALLBACK process_service_event (DWORD event, DWORD event_, LPVOID data, LPVOID context);

} // extern "C"

std::variant<service_configuration, application_error> retrieve_configuration_ ();

application_error not_service ();
application_error failed_to_subscribe_to_events ();
application_error failed_to_subscribe_to_events_ ();
application_error failed_to_set_state ();
application_error failed_to_connect_to_scm ();
application_error failed_to_enumerate_services ();
application_error failed_to_describe_service ();

#if not defined background_prevent_failed_to_connect_to_scm_exception
#if defined NDEBUG
#define background_prevent_failed_to_connect_to_scm_exception true
#else
#define background_prevent_failed_to_connect_to_scm_exception false
#endif
#endif
#if not background_prevent_failed_to_connect_to_scm_exception
bool should_try_running_as_a_service ();
#endif

} // namespace

service_platform_windows::service_platform_windows (QObject * const parent)
    : service_platform (parent)
{}

service_platform_windows::~service_platform_windows ()
{
    if (instance == nullptr)
        return;
    QMutexLocker locker (& mutex);
    instance = nullptr;
}

bool service_platform_windows::check ()
{
    #if background_prevent_failed_to_connect_to_scm_exception
    // The only certain way to tell whether the process is a service or not
    // is to go ahead and actually try running as a service or get an error.
    return true;
    #else
    // But there is an annoyance about 'StartServiceCtrlDispatcher ()'.
    // In case of 'ERROR_FAILED_SERVICE_CONTROLLER_CONNECT' the system throws an exception and handles it internally.
    // For someone debugging a console application in the IDE this renders an issue or/and halts the debugger.
    // Configuring the first chance exception ignore list every time is not an option.
    return should_try_running_as_a_service ();
    #endif
}

void service_platform_windows::start ()
{
    connect (
        this, & service_platform_windows::event_received_,
        this, & service_platform_windows::process_event,
        Qt::QueuedConnection
    );

    auto * const thread (QThread::create (& run_service));
    connect (thread, & QThread::finished, thread, & QThread::deleteLater);
    connect (
        this, & service_platform_windows::proceed_, this,
        [this] ()
        {
            QMutexLocker locker (& mutex);
            if (starting != starting_sequence::checkpoint_1)
                return;
            starting = starting_sequence::done;
            if (run_service_result->failed ())
            {
                instance = nullptr;
                const auto error (run_service_result->error ());
                locker.unlock ();
                Q_EMIT failed_to_start (error);
                return;
            }
            state =
            {
                SERVICE_WIN32_OWN_PROCESS,
                SERVICE_START_PENDING,
                SERVICE_ACCEPT_STOP bitor SERVICE_ACCEPT_PRESHUTDOWN,
                NO_ERROR,
                0,
                0, 0
            };
            if (not SetServiceStatus (service, & state))
            {
                instance = nullptr;
                service = nullptr;
                locker.unlock ();
                //thread->terminate (); // Doing so is not as structured.
                Q_EMIT failed_to_start (failed_to_subscribe_to_events ());
                return;
            }
            locker.unlock ();
            Q_EMIT started ();
        },
        Qt::QueuedConnection
    );
    connect (
        thread, & QThread::finished,
        this,
        [this] ()
        {
            if (instance == nullptr)
                return;
            if (stopping == stopping_sequence::stop)
            {
                stopping = stopping_sequence::done;
                {
                    QMutexLocker locker (& mutex);
                    instance = nullptr;
                }
                Q_EMIT stopped ();
                return;
            }
            QMutexLocker locker (& mutex);
            if (starting == starting_sequence::checkpoint_2)
            {
                starting = starting_sequence::done;
                instance = nullptr;
                const auto error (
                    run_service_result->failed ()
                    ? run_service_result->error ()
                    : failed_to_subscribe_to_events_ ()
                );
                locker.unlock ();
                Q_EMIT failed_to_start (error);
                return;
            }
            else if (starting != starting_sequence::done)
            {
                starting = starting_sequence::done;
                instance = nullptr;
                service = nullptr;
                run_service_result->value = std::nullopt;
                locker.unlock ();
                Q_EMIT failed_to_start (failed_to_subscribe_to_events_ ());
                return;
            }
            // 'StartServiceCtrlDispatcher ()' does not unblock by itself.
            // The service thread should not finish on its own.
            // But the service process may be tampered with.
            instance = nullptr;
            service = nullptr;
            locker.unlock ();
            Q_EMIT event_received (
                application_system_event
                { // c++20 designated initializers
                    /*.action = */application_system_event::stop,
                    /*.name = */QStringLiteral ("close") // "connection loss" ?
                }
            );
        },
        Qt::QueuedConnection
    );
    instance = this;
    thread->start ();
    // 'QThread' has no developed error model.
    // But the underlying 'CreateThread ()' may fail.
    if (thread->isRunning ())
        return;
    instance = nullptr;
    Q_EMIT failed_to_start (failed_to_subscribe_to_events_ ());
}

void service_platform_windows::stop ()
{
    if (service == nullptr or state.dwCurrentState == SERVICE_STOPPED)
    {
        Q_EMIT stopped ();
        return;
    }
    stopping = stopping_sequence::stop;
    // There is no static guarantee that 'StartServiceCtrlDispatcher ()' will actually unblock.
    // A timer would not hurt.
    state.dwCurrentState = SERVICE_STOPPED;
    state.dwControlsAccepted = 0;
    auto * const service_ (service);
    service = nullptr;
    if (not SetServiceStatus (service_, & state))
    {
        // How to make 'StartServiceCtrlDispatcher ()' return control
        // when one can not set the state even to stopped?
        // Terminate the thread and wait for 'finished ()' or just leave it as is — does not matter:
        // the application will exit soon anyway.
        qCWarning (category).noquote () << text::with_last_error (QStringLiteral (
            "Failed to stop running as a service"
        ));
        stopping = stopping_sequence::done;
        {
            QMutexLocker locker (& mutex);
            instance = nullptr;
        }
        Q_EMIT stopped ();
    }
}

void service_platform_windows::set_state_serving ()
{
    if (service == nullptr)
    {
        Q_EMIT state_serving_set ();
        return;
    }
    state.dwCurrentState = SERVICE_RUNNING;
    if (not SetServiceStatus (service, & state))
    {
        Q_EMIT failed_to_set_state_serving (failed_to_set_state ());
        return;
    }
    Q_EMIT state_serving_set ();
}

void service_platform_windows::set_state_stopping ()
{
    if (service == nullptr)
    {
        Q_EMIT state_stopping_set ();
        return;
    }
    state.dwCurrentState = SERVICE_STOP_PENDING;
    if (not SetServiceStatus (service, & state))
        qCWarning (category).noquote () << text::with_last_error (QStringLiteral (
            "Failed to set service state"
        ));
    Q_EMIT state_stopping_set ();
}

void service_platform_windows::set_state_stopped (const int exit_code)
{
    state.dwWin32ExitCode = static_cast<DWORD> (exit_code);
    Q_EMIT state_stopped_set ();
}

void service_platform_windows::retrieve_configuration ()
{
    const auto result (retrieve_configuration_ ());
    if (std::holds_alternative<service_configuration> (result))
        emit configuration_retrieved (std::get<service_configuration> (result));
    else
        emit failed_to_retrieve_configuration (std::get<application_error> (result));
}

void service_platform_windows::process_event (const unsigned long event)
{
    const auto event_name = [] (const unsigned long event)
    {
        switch (event)
        {
            case SERVICE_CONTROL_STOP : return QStringLiteral ("stop");
            case SERVICE_CONTROL_PRESHUTDOWN : return QStringLiteral ("shutdown");
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

void run_service ()
{
    const QString name;
    const SERVICE_TABLE_ENTRYW services []
    {
        { reinterpret_cast<LPWSTR> (const_cast<QChar *> (name.unicode ())), & run_service_ },
        { nullptr, nullptr }
    };
    // Blocks until the service is stopped.
    const auto result (StartServiceCtrlDispatcherW (services));
    QMutexLocker locker (& mutex);
    if (instance == nullptr)
        return;
    if (starting == starting_sequence::none)
    {
        starting = starting_sequence::checkpoint_2;
        if (result)
            run_service_result->value = std::nullopt;
        else
            run_service_result->value = GetLastError () == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT
            ? not_service () : failed_to_subscribe_to_events ();
        return;
    }
    if (result)
        return;
    qCWarning (category).noquote () << text::with_last_error (QStringLiteral (
        "Failed to run as a service"
    ));
}

extern "C"
{

// Called in a thread spawned by the Service Control Manager and stopped right after the function returns.
void QT_WIN_CALLBACK run_service_ (const DWORD argc, LPWSTR * const argv)
{
    Q_UNUSED (argc) Q_UNUSED (argv)

    QMutexLocker locker (& mutex);
    if (instance == nullptr)
        return;
    if (starting != starting_sequence::none)
        return;
    starting = starting_sequence::checkpoint_1;
    // The documentation emphasizes the need to call 'RegisterServiceCtrlHandler ()' _immediately_.
    // Though nothing stops one from doing this later in the application thread.
    const auto result (RegisterServiceCtrlHandlerExW (nullptr, & process_service_event, nullptr));
    if (result != nullptr)
    {
        service = result;
        run_service_result->value = std::nullopt;
    }
    else // Normally this never happens.
        run_service_result->value = failed_to_subscribe_to_events ();
    Q_EMIT instance->proceed_ ();
}

// Called in the thread controlled by the implementation.
DWORD QT_WIN_CALLBACK process_service_event (
    const DWORD event,
    const DWORD event_, const LPVOID data,
    const LPVOID context
)
{
    Q_UNUSED (event_) Q_UNUSED (data) Q_UNUSED (context)

    switch (event)
    {
        case SERVICE_CONTROL_STOP :
        case SERVICE_CONTROL_PRESHUTDOWN :
        {
            QMutexLocker locker (& mutex);
            if (instance == nullptr)
                return NO_ERROR;
            Q_EMIT instance->event_received_ (event);
            return NO_ERROR;
        }

        case SERVICE_CONTROL_INTERROGATE : return NO_ERROR;

        default : return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

} // extern "C"

std::variant<service_configuration, application_error> retrieve_configuration_ ()
{
    const auto manager (OpenSCManager (nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
    if (manager == nullptr)
        return failed_to_connect_to_scm ();
    const auto clean_up_1 (qScopeGuard (
        [manager] ()
        {
            if (not CloseServiceHandle (manager))
                qCWarning (category).noquote () << text::with_last_error (QStringLiteral (
                    "Failed to close the Service Control Manager"
                ));
        }
    ));

    DWORD buffer_size (0);
    DWORD service_count (0);
    if (
        EnumServicesStatusExW (
            manager, SC_ENUM_PROCESS_INFO,
            SERVICE_WIN32_OWN_PROCESS, SERVICE_ACTIVE,
            nullptr, 0,
            & buffer_size, & service_count,
            nullptr, nullptr
        )
    )
        return application_error
        { // c++20 designated initializers
            /*.error =*/application_error::failed_to_retrieve_configuration,
            /*.text =*/QStringLiteral (
                "Failed to retrieve service configuration. "
                "Failed to enumerate services: failed to create a buffer."
            )
        };
    if (GetLastError () != ERROR_MORE_DATA)
        return failed_to_enumerate_services ();
    QByteArray buffer_1 (static_cast<int> (buffer_size), Qt::Uninitialized);
    if (
        not EnumServicesStatusExW (
            manager, SC_ENUM_PROCESS_INFO,
            SERVICE_WIN32_OWN_PROCESS, SERVICE_ACTIVE,
            reinterpret_cast<LPBYTE> (buffer_1.data ()), buffer_size,
            & buffer_size, & service_count,
            nullptr, nullptr
        )
    )
        return failed_to_enumerate_services ();
    const auto * service_1 (
        [] (auto buffer, const auto service_count)
        {
            const auto process_id (QCoreApplication::applicationPid ());
            const auto * result (reinterpret_cast<const ENUM_SERVICE_STATUS_PROCESSW *> (buffer.constData ()));
            for (DWORD i (0); i < service_count; ++i, ++result)
            {
                if (result->ServiceStatusProcess.dwProcessId == process_id)
                    return result;
            }
            return static_cast<const ENUM_SERVICE_STATUS_PROCESSW *> (nullptr);
        } (buffer_1, service_count)
    );
    if (service_1 == nullptr)
        return application_error
        { // c++20 designated initializers
            /*.error =*/application_error::failed_to_retrieve_configuration,
            /*.text =*/QStringLiteral (
                "Failed to retrieve service configuration. "
                "Failed to find the service."
            )
        };

    const auto service_2 (OpenServiceW (manager, service_1->lpServiceName, SERVICE_QUERY_CONFIG));
    if (service_2 == nullptr)
        return failed_to_describe_service ();
    const auto clean_up_2 (qScopeGuard (
        [service_2] ()
        {
            if (not CloseServiceHandle (service_2))
                qCWarning (category).noquote () << text::with_last_error (QStringLiteral (
                    "Failed to close the service"
                ));
        }
    ));
    buffer_size = 0;
    if (QueryServiceConfigW (service_2, nullptr, 0, & buffer_size))
        return application_error
        { // c++20 designated initializers
            /*.error =*/application_error::failed_to_retrieve_configuration,
            /*.text =*/QStringLiteral (
                "Failed to retrieve service configuration. "
                "Failed to describe the service: failed to create a buffer."
            )
        };
    if (GetLastError () != ERROR_INSUFFICIENT_BUFFER)
        return failed_to_describe_service ();
    QByteArray buffer_2 (static_cast<int> (buffer_size), Qt::Uninitialized);
    if (
        not QueryServiceConfigW (
            service_2,
            reinterpret_cast<QUERY_SERVICE_CONFIGW *> (buffer_2.data ()),
            buffer_size,
            & buffer_size
        )
    )
        return failed_to_describe_service ();
    buffer_size = 0;
    if (QueryServiceConfig2W (service_2, SERVICE_CONFIG_DESCRIPTION, nullptr, 0, & buffer_size))
        return application_error
        { // c++20 designated initializers
            /*.error =*/application_error::failed_to_retrieve_configuration,
            /*.text =*/QStringLiteral (
                "Failed to retrieve service configuration. "
                "Failed to describe the service: failed to create a buffer."
            )
        };
    if (GetLastError () != ERROR_INSUFFICIENT_BUFFER)
        return failed_to_describe_service ();
    QByteArray buffer_3 (static_cast<int> (buffer_size), Qt::Uninitialized);
    if (
        not QueryServiceConfig2W (
            service_2,
            SERVICE_CONFIG_DESCRIPTION,
            reinterpret_cast<LPBYTE> (buffer_3.data ()),
            buffer_size,
            & buffer_size
        )
    )
        return failed_to_describe_service ();

    const auto & result_1 (* reinterpret_cast<const QUERY_SERVICE_CONFIGW *> (buffer_2.constData ()));
    const auto & result_2 (* reinterpret_cast<const SERVICE_DESCRIPTIONW *> (buffer_3.constData ()));
    return service_configuration
    { // c++20 designated initializers
        /*name =*/QString (reinterpret_cast<const QChar *> (service_1->lpServiceName)),
        /*description =*/result_2.lpDescription != Q_NULLPTR
        ? QString (reinterpret_cast<const QChar *> (result_2.lpDescription)) : QString (),
        /*executable =*/QString (reinterpret_cast<const QChar *> (result_1.lpBinaryPathName)),
        /*user =*/QString (reinterpret_cast<const QChar *> (result_1.lpServiceStartName))
    };
}

bool run_service_result_::failed () const
{
    return value.has_value ();
}

application_error run_service_result_::error ()
{
    application_error result (std::move (value.value ()));
    value = std::nullopt;
    return result;
}

application_error not_service ()
{
    return application_error
    { // c++20 designated initializers
        /*.error =*/application_error::not_service,
        /*.text =*/text::with_last_error (QStringLiteral (
            "Failed to run as a service. "
            "This process is not a service spawned by the system"
        ))
    };
}

application_error failed_to_subscribe_to_events ()
{
    return application_error
    { // c++20 designated initializers
        /*.error =*/application_error::failed_to_run,
        /*.text =*/text::with_last_error (QStringLiteral (
            "Failed to run as a service. "
            "Failed to subscribe to service events"
        ))
    };
}

application_error failed_to_subscribe_to_events_ ()
{
    return application_error
    { // c++20 designated initializers
        /*.error =*/application_error::failed_to_run,
        /*.text =*/QStringLiteral (
            "Failed to run as a service. "
            "Failed to subscribe to service events."
        )
    };
}

application_error failed_to_set_state ()
{
    return application_error
    { // c++20 designated initializers
        /*.error =*/application_error::failed_to_run,
        /*.text =*/text::with_last_error (QStringLiteral (
            "Failed to run as a service. "
            "Failed to set service state"
        ))
    };
}

application_error failed_to_connect_to_scm ()
{
    return application_error
    { // c++20 designated initializers
        /*.error =*/application_error::failed_to_retrieve_configuration,
        /*.text =*/text::with_last_error (QStringLiteral (
            "Failed to retrieve service configuration. "
            "Failed to connect to the Service Control Manager"
        ))
    };
}

application_error failed_to_enumerate_services ()
{
    return application_error
    { // c++20 designated initializers
        /*.error =*/application_error::failed_to_retrieve_configuration,
        /*.text =*/text::with_last_error (QStringLiteral (
            "Failed to retrieve service configuration. "
            "Failed to enumerate services"
        ))
    };
}

application_error failed_to_describe_service ()
{
    return application_error
    { // c++20 designated initializers
        /*.error =*/application_error::failed_to_retrieve_configuration,
        /*.text =*/text::with_last_error (QStringLiteral (
            "Failed to retrieve service configuration. "
            "Failed to describe the service"
        ))
    };
}

} // namespace

#if not background_prevent_failed_to_connect_to_scm_exception

#include <TlHelp32.h>

namespace
{

bool should_try_running_as_a_service ()
{
    const auto snapshot (CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0));
    if (snapshot == reinterpret_cast<void *> (static_cast<quintptr> (-1))) // INVALID_HANDLE_VALUE
        return true;
    const auto clean_up (qScopeGuard (
        [snapshot] ()
        {
            CloseHandle (snapshot);
        }
    ));

    auto process_id (QCoreApplication::applicationPid ());
    PROCESSENTRY32W process;
    process.dwSize = sizeof (PROCESSENTRY32W);
    bool result (false);
    Q_FOREVER
    {
        for (result = Process32FirstW (snapshot, & process); result; result = Process32NextW (snapshot, & process))
        {
            if (process.th32ProcessID == process_id)
                break;
        }
        if (not result)
            return GetLastError () != ERROR_NO_MORE_FILES;
        if (QString (reinterpret_cast<const QChar *> (process.szExeFile)) == QStringLiteral ("services.exe"))
            return true;
        process_id = process.th32ParentProcessID;
    }
}

} // namespace

#endif

background_service_platform_plugin_windows::background_service_platform_plugin_windows (QObject * const parent)
    : service_platform_plugin (parent)
{}

unsigned int background_service_platform_plugin_windows::order () const
{
    return 99;
}

bool background_service_platform_plugin_windows::detect ()
{
    return true;
}

service_platform * background_service_platform_plugin_windows::create (QObject * const parent)
{
    return new service_platform_windows (parent);
}

} // namespace background

Q_IMPORT_PLUGIN (background_service_platform_plugin_windows)
