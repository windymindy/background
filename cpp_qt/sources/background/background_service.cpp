#include "background_service.hpp"

#include <functional>
#include <vector>
#include <deque>
#include <map>
#include <cassert>

#include <QtCore/QPointer>
#include <QtCore/QMetaMethod>
#include <QtCore/QPluginLoader>
#include <QtCore/QLoggingCategory>

#include "background_datatypes.hpp"
#include "background_application_controller.hpp"
#include "background_application_controller_qt.hpp"
#include "background_service_platform.hpp"
#include "background_console_platform.hpp"

static Q_LOGGING_CATEGORY (category, "background.service")

namespace background
{

enum class starting_sequence
{
    none,
    set_up_application_controller,

    set_up_service_platform,
    start_service_platform,
    retrieve_configuration,
    start_serving_1,
    set_service_state_serving,

    set_up_console_platform,
    start_console_platform,
    start_serving_2,

    set_state_serving,
    done
};

enum class stopping_sequence
{
    none,
    set_up_application_controller,

    set_service_state_stopping,
    stop_serving,
    set_service_state_stopped,
    stop_service_platform,

    stop_console_platform,

    exit_application,
    set_state_stopped,
    done
};

enum class proceeding_state
{
    none,
    starting,
    started,
    stopping,
    stopped,
    failed
};

enum class control_state
{
    none,
    queueing,
    processing
};

enum class proceed_result
{
    continue_,
    nothing_to_do,
    lost_control,
    destroyed
};

class service_implementation
{
    public :
    explicit service_implementation (service * service);

    protected :
    serving_state state;
    std::optional<bool> running_as_service;
    std::optional<service_configuration> configuration;
    std::optional<service_error> error;
    std::optional<service_error> error_;
    int exit_code;

    bool with_stop_starting;
    bool with_running_as_console_application;
    bool no_running_as_service;
    bool no_retrieving_configuration;

    protected :
    starting_sequence starting;
    stopping_sequence stopping;
    proceeding_state proceeding;
    control_state control;
    bool regain_control;
    bool processing_recoverable_error;
    bool error_ignored;
    bool exiting_abruptly;
    std::deque<service_system_event> system_events;

    application_controller * application;
    service_platform * service_platform;
    console_platform * console_platform;

    protected :
    void proceed_from_event_loop ();
    void check_proceeding_and_lose_control ();
    void proceed ();
    proceed_result proceed_starting ();
    proceed_result proceed_stopping ();
    proceed_result process_error ();
    void process_system_event ();

    void set_up_application_controller ();
    void shut_down_before_application_exits ();

    void process_system_event_received (const service_system_event & event);

    void set_up_service_platform ();
    void process_service_platform_started ();
    void process_service_platform_failed_to_start (const service_error & error);
    void process_service_platform_stopped ();
    void process_service_state_serving_set ();
    void process_failed_to_set_service_state_serving (const service_error & error);
    void process_service_state_stopping_set ();
    void process_service_state_stopped_set ();
    void process_service_configuration_retrieved (const service_configuration & configuration);
    void process_failed_to_retrieve_service_configuration (const service_error & error);

    void set_up_console_platform ();
    void process_console_platform_started ();
    void process_console_platform_failed_to_start (const service_error & error);
    void process_console_platform_stopped ();

    template <class T> static std::vector<T *> plugins ();

    protected :
    service * const this_;
    friend class service;
};

service::service (QObject * const parent)
    : QObject (parent),
    this_ (new service_implementation (this))
{}

service_implementation::service_implementation (service * const service)
    : state (
        { // c++20 designated initializers
            /*.state = */service_state::none,
            /*.target_service_state = */target_service_state::none
        }
    ),
    running_as_service (std::nullopt),
    configuration (std::nullopt),
    error (std::nullopt),
    error_ (std::nullopt),
    exit_code (0),
    with_stop_starting (false),
    with_running_as_console_application (false),
    no_running_as_service (false),
    no_retrieving_configuration (false),
    starting (starting_sequence::none),
    stopping (stopping_sequence::none),
    proceeding (proceeding_state::none),
    control (control_state::none),
    regain_control (false),
    application (nullptr),
    service_platform (nullptr),
    console_platform (nullptr),
    exiting_abruptly (false),
    processing_recoverable_error (false),
    error_ignored (false),
    this_ (service)
{}

service::~service ()
{
    assert (this_->state.stopped () or this_->state.none ());
}

void service::run ()
{
    assert (this_->state.none ());
    if (not this_->state.none ())
        return;
    this_->state.target_state = target_service_state::serving;
    this_->proceed_from_event_loop ();
}

void service::shut_down ()
{
    if (this_->state.stopped ())
        return;
    if (this_->state.target_state == target_service_state::stopped)
        return;
    this_->state.target_state = target_service_state::stopped;
    this_->proceed_from_event_loop ();
}

void service::set_started ()
{
    // Allow before the signal was emitted?
    if (
        this_->starting != starting_sequence::start_serving_1
        and this_->starting != starting_sequence::start_serving_2
        or this_->proceeding != proceeding_state::starting
    )
        return;
    this_->proceeding = proceeding_state::started;
    this_->proceed_from_event_loop ();
}

void service::set_failed_to_start ()
{
    if (
        this_->starting != starting_sequence::start_serving_1
        and this_->starting != starting_sequence::start_serving_2
        or this_->proceeding != proceeding_state::starting
    )
        return;
    this_->state.target_state = target_service_state::stopped;
    this_->proceeding = proceeding_state::failed;
    this_->proceed_from_event_loop ();
}

void service::set_stopped ()
{
    if (
        this_->stopping != stopping_sequence::stop_serving
        or this_->proceeding != proceeding_state::stopping
    )
        return;
    this_->proceeding = proceeding_state::stopped;
    this_->proceed_from_event_loop ();
}

void service::ignore_error ()
{
    if (not this_->processing_recoverable_error)
        return;
    this_->error_ignored = true;
}

serving_state service::state () const
{
    return this_->state;
}

std::optional<bool> service::running_as_service () const
{
    return this_->running_as_service;
}

const std::optional<service_configuration> & service::configuration () const
{
    return this_->configuration;
}

const std::optional<service_error> & service::error () const
{
    return this_->error;
}

int service::exit_code () const
{
    return this_->exit_code;
}

void service::set_exit_code (const int exit_code)
{
    this_->exit_code = exit_code;
}

bool service::with_stop_starting () const
{
    return this_->with_stop_starting;
}

service & service::set_with_stop_starting ()
{
    assert (this_->state.none ());
    if (this_->state.none ())
        this_->with_stop_starting = true;
    return * this;
}

bool service::with_running_as_console_application () const
{
    return this_->with_running_as_console_application;
}

service & service::set_with_running_as_console_application ()
{
    assert (this_->state.none ());
    if (this_->state.none ())
        this_->with_running_as_console_application = true;
    return * this;
}

bool service::no_running_as_service () const
{
    return this_->no_running_as_service;
}

service & service::set_no_running_as_service ()
{
    assert (this_->state.none ());
    if (this_->state.none ())
        this_->no_running_as_service = true;
    return * this;
}

bool service::no_retrieving_configuration () const
{
    return this_->no_retrieving_configuration;
}

service & service::set_no_retrieving_configuration ()
{
    assert (this_->state.none ());
    if (this_->state.none ())
        this_->no_retrieving_configuration = true;
    return * this;
}

void service_implementation::proceed_from_event_loop ()
{
    switch (control)
    {
        case control_state::none : break;
        case control_state::queueing : return;
        case control_state::processing :
        regain_control = true;
        return;
    }
    control = control_state::queueing;
    QMetaObject::invokeMethod (
        this_,
        std::bind (& service_implementation::proceed, this),
        Qt::QueuedConnection
    );
}

void service_implementation::check_proceeding_and_lose_control ()
{
    control = control_state::none;
    if (not regain_control)
        return;
    regain_control = false;
    proceed_from_event_loop ();
}

// A higher level routine description.
// This is a critical section:
// even where the user regains execution control, it won't enter more than once.
// The handling priority is hardcoded.
void service_implementation::proceed ()
{
    control = control_state::processing;
    Q_FOREVER
    {
        regain_control = false;

        if (not system_events.empty () and stopping < stopping_sequence::exit_application)
        {
            process_system_event ();
            continue;
        }

        if (error_.has_value ())
        {
            switch (process_error ())
            {
                case proceed_result::continue_ : continue;
                case proceed_result::destroyed : return;
                default : Q_UNREACHABLE (); return;
            }
        }

        switch (state.target_state)
        {
            case target_service_state::serving :
            switch (proceed_starting ())
            {
                case proceed_result::continue_ : continue;
                case proceed_result::nothing_to_do : break;
                case proceed_result::lost_control : return;
                case proceed_result::destroyed : return;
                default : Q_UNREACHABLE (); return;
            }
            break;

            case target_service_state::stopped :
            switch (proceed_stopping ())
            {
                case proceed_result::continue_ : continue;
                case proceed_result::nothing_to_do : break;
                case proceed_result::lost_control : return;
                case proceed_result::destroyed : return;
                default : Q_UNREACHABLE (); return;
            }
            break;

            case target_service_state::none : break;
        }

        control = control_state::none;
        regain_control = false;
        break;
    }
}

// The asynchronous routine is expressed as a state switch so that
// it is read from a single place and is perceived consequent.
// The state controls what has already been done and
// what to be done to achieve the target state in different scenarios.
proceed_result service_implementation::proceed_starting ()
{
    switch (starting)
    {
        case starting_sequence::none :
        // Logging is losing control.
        // Anything might happen inside a handler set with 'qInstallMessageHandler ()'.
        // The target state might change. This instance might even get destroyed.
        // But no code is safe from that.
        // The paranoid approach of checking that this instance still exists after
        // every call to another module does not scale.
        qCInfo (category, "Starting...");
        starting = starting_sequence::set_up_application_controller;
        [[ fallthrough ]];

        case starting_sequence::set_up_application_controller :
        // 'QObject::connect' can also lose control via 'QObject::connectNotify'.
        set_up_application_controller ();
        if (not no_running_as_service)
            starting = starting_sequence::set_up_service_platform;
        else
            starting = starting_sequence::set_up_console_platform;
        return proceed_result::continue_;

        case starting_sequence::set_up_service_platform :
        switch (proceeding)
        {
            case proceeding_state::none :
            set_up_service_platform ();
            return proceed_result::continue_;

            case proceeding_state::started :
            proceeding = proceeding_state::none;
            starting = starting_sequence::start_service_platform;
            break;

            case proceeding_state::failed :
            proceeding = proceeding_state::none;
            starting = starting_sequence::set_up_console_platform;
            return proceed_result::continue_;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }
        [[ fallthrough ]];

        case starting_sequence::start_service_platform :
        switch (proceeding)
        {
            case proceeding_state::none :
            proceeding = proceeding_state::starting;
            service_platform->start ();
            return proceed_result::continue_;

            case proceeding_state::starting : return proceed_result::nothing_to_do;

            case proceeding_state::started :
            proceeding = proceeding_state::none;
            starting = starting_sequence::retrieve_configuration;
            break;

            case proceeding_state::failed :
            proceeding = proceeding_state::none;
            starting = starting_sequence::set_up_console_platform;
            return proceed_result::continue_;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }
        [[ fallthrough ]];

        case starting_sequence::retrieve_configuration :
        switch (proceeding)
        {
            case proceeding_state::none :
            if (no_retrieving_configuration)
            {
                starting = starting_sequence::start_serving_1;
                break;
            }
            proceeding = proceeding_state::starting;
            service_platform->retrieve_configuration ();
            return proceed_result::continue_;

            case proceeding_state::starting : return proceed_result::nothing_to_do;

            case proceeding_state::started :
            proceeding = proceeding_state::none;
            starting = starting_sequence::start_serving_1;
            break;

            case proceeding_state::failed :
            proceeding = proceeding_state::none;
            starting = starting_sequence::start_serving_1;
            break;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }
        [[ fallthrough ]];

        case starting_sequence::start_serving_1 :
        switch (proceeding)
        {
            case proceeding_state::none :
            state.state = service_state::starting;
            running_as_service = true;
            qCInfo (category, "Start serving.");
            proceeding = proceeding_state::starting;
            if (not this_->isSignalConnected (QMetaMethod::fromSignal (& service::start)))
                return proceed_result::continue_;
            // Control is lost emitting a signal, but the sequence should proceed.
            // The user might reenter the event loop in the slot and not return control for a while,
            // Therefore, there is no use doing anything directly after emitting.
            // The control will be returned through the public methods.
            {
                check_proceeding_and_lose_control ();
                const QPointer<const service> this_exists (this_);
                Q_EMIT this_->start ();
                if (this_exists.isNull ())
                    return proceed_result::destroyed;
            }
            return proceed_result::lost_control;

            case proceeding_state::starting : return proceed_result::nothing_to_do;

            case proceeding_state::started :
            proceeding = proceeding_state::none;
            starting = starting_sequence::set_service_state_serving;
            break;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }
        [[ fallthrough ]];

        // Could just ignore the failure of setting the service state to serving since it is already running.
        // But notifying the system of successful initialization is one of the key steps and responsibilities of a service.
        // So, handling this explicitly is a better alternative to the process being killed after timeout by the operating system.
        case starting_sequence::set_service_state_serving :
        switch (proceeding)
        {
            case proceeding_state::none :
            proceeding = proceeding_state::starting;
            service_platform->set_state_serving ();
            return proceed_result::continue_;

            case proceeding_state::starting : return proceed_result::nothing_to_do;

            case proceeding_state::started :
            proceeding = proceeding_state::none;
            starting = starting_sequence::set_state_serving;
            return proceed_result::continue_;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }

        case starting_sequence::set_up_console_platform :
        switch (proceeding)
        {
            case proceeding_state::none :
            if (service_platform != nullptr)
            {
                if (service_platform->parent () == this_)
                    service_platform->deleteLater ();
                service_platform = nullptr;
            }
            set_up_console_platform ();
            return proceed_result::continue_;

            case proceeding_state::started :
            proceeding = proceeding_state::none;
            starting = starting_sequence::start_console_platform;
            break;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }
        [[ fallthrough ]];

        case starting_sequence::start_console_platform :
        switch (proceeding)
        {
            case proceeding_state::none :
            proceeding = proceeding_state::starting;
            console_platform->start ();
            return proceed_result::continue_;

            case proceeding_state::starting : return proceed_result::nothing_to_do;

            case proceeding_state::started :
            proceeding = proceeding_state::none;
            starting = starting_sequence::start_serving_2;
            break;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }
        [[ fallthrough ]];

        case starting_sequence::start_serving_2 :
        switch (proceeding)
        {
            case proceeding_state::none :
            state.state = service_state::starting;
            running_as_service = false;
            qCInfo (category, "Start serving as a console application.");
            proceeding = proceeding_state::starting;
            if (not this_->isSignalConnected (QMetaMethod::fromSignal (& service::start)))
                return proceed_result::continue_;
            {
                check_proceeding_and_lose_control ();
                const QPointer<const service> this_exists (this_);
                Q_EMIT this_->start ();
                if (this_exists.isNull ())
                    return proceed_result::destroyed;
            }
            return proceed_result::lost_control;

            case proceeding_state::starting : return proceed_result::nothing_to_do;

            case proceeding_state::started :
            proceeding = proceeding_state::none;
            starting = starting_sequence::set_state_serving;
            return proceed_result::continue_;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }

        case starting_sequence::set_state_serving :
        state.state = service_state::serving;
        state.target_state = target_service_state::none;
        qCInfo (category, "Serving...");
        starting = starting_sequence::done;
        if (not this_->isSignalConnected (QMetaMethod::fromSignal (& service::state_changed)))
            return proceed_result::continue_;
        {
            check_proceeding_and_lose_control ();
            const QPointer<const service> this_exists (this_);
            Q_EMIT this_->state_changed ();
            if (this_exists.isNull ())
                return proceed_result::destroyed;
        }
        return proceed_result::lost_control;

        case starting_sequence::done :
        default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
    }
}

proceed_result service_implementation::proceed_stopping ()
{
    switch (stopping)
    {
        case stopping_sequence::none :
        switch (starting)
        {
            case starting_sequence::done :
            state.state = service_state::stopping;
            stopping = stopping_sequence::stop_serving;
            break;

            case starting_sequence::start_serving_1 :
            case starting_sequence::start_serving_2 :
            switch (proceeding)
            {
                case proceeding_state::starting :
                if (not with_stop_starting)
                    return proceed_result::nothing_to_do;
                break;

                case proceeding_state::started : break;

                case proceeding_state::failed :
                state.state = service_state::stopping;
                qCInfo (category, "Failed to start serving. Stopping...");
                proceeding = proceeding_state::none;
                stopping = stopping_sequence::stop_serving;
                return proceed_result::continue_;

                default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
            }
            state.state = service_state::stopping;
            proceeding = proceeding_state::none;
            if (service_platform != nullptr)
                stopping = stopping_sequence::set_service_state_stopping;
            else
                stopping = stopping_sequence::stop_serving;
            break;

            case starting_sequence::set_service_state_serving :
            switch (proceeding)
            {
                case proceeding_state::starting : return proceed_result::nothing_to_do;

                case proceeding_state::started :
                case proceeding_state::failed : break;

                default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
            }
            state.state = service_state::stopping;
            proceeding = proceeding_state::none;
            stopping = stopping_sequence::set_service_state_stopping;
            break;

            case starting_sequence::retrieve_configuration :
            switch (proceeding)
            {
                case proceeding_state::starting : return proceed_result::nothing_to_do;

                case proceeding_state::started :
                case proceeding_state::failed : break;

                default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
            }
            state.state = service_state::stopped;
            proceeding = proceeding_state::none;
            stopping = stopping_sequence::set_service_state_stopped;
            break;

            case starting_sequence::start_service_platform :
            switch (proceeding)
            {
                case proceeding_state::starting : return proceed_result::nothing_to_do;

                case proceeding_state::started :
                state.state = service_state::stopped;
                proceeding = proceeding_state::none;
                stopping = stopping_sequence::set_service_state_stopped;
                break;

                case proceeding_state::failed :
                state.state = service_state::stopped;
                proceeding = proceeding_state::none;
                stopping = stopping_sequence::exit_application;
                break;

                default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
            }
            break;

            case starting_sequence::start_console_platform :
            switch (proceeding)
            {
                case proceeding_state::starting : return proceed_result::nothing_to_do;

                case proceeding_state::started :
                state.state = service_state::stopped;
                proceeding = proceeding_state::none;
                stopping = stopping_sequence::stop_console_platform;
                break;

                case proceeding_state::failed :
                state.state = service_state::stopped;
                proceeding = proceeding_state::none;
                stopping = stopping_sequence::exit_application;
                break;

                default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
            }
            break;

            case starting_sequence::set_up_service_platform :
            case starting_sequence::set_up_console_platform :
            //case starting_sequence::set_up_application_controller :
            state.state = service_state::stopped;
            proceeding = proceeding_state::none;
            stopping = stopping_sequence::exit_application;
            break;

            case starting_sequence::none :
            state.state = service_state::stopped;
            stopping = stopping_sequence::set_up_application_controller;
            break;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }
        qCInfo (category, "Stopping...");
        return proceed_result::continue_;

        case stopping_sequence::set_up_application_controller :
        set_up_application_controller ();
        stopping = stopping_sequence::exit_application;
        return proceed_result::continue_;

        case stopping_sequence::set_service_state_stopping :
        switch (proceeding)
        {
            case proceeding_state::none :
            proceeding = proceeding_state::stopping;
            // On the contrary, the failure of setting the service state to stopping or stopped is of no interest.
            // So the error is not in the model, and the platform implementation should just log it.
            service_platform->set_state_stopping ();
            return proceed_result::continue_;

            case proceeding_state::stopping : return proceed_result::nothing_to_do;

            case proceeding_state::stopped :
            proceeding = proceeding_state::none;
            stopping = stopping_sequence::stop_serving;
            return proceed_result::continue_;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }

        case stopping_sequence::stop_serving :
        switch (proceeding)
        {
            case proceeding_state::none :
            state.state = service_state::stopping;
            qCInfo (category, "Stop serving.");
            proceeding = proceeding_state::stopping;
            if (not this_->isSignalConnected (QMetaMethod::fromSignal (& service::stop)))
                return proceed_result::continue_;
            {
                check_proceeding_and_lose_control ();
                const QPointer<const service> this_exists (this_);
                Q_EMIT this_->stop ();
                if (this_exists.isNull ())
                    return proceed_result::destroyed;
            }
            return proceed_result::lost_control;

            case proceeding_state::stopping : return proceed_result::nothing_to_do;

            case proceeding_state::stopped :
            proceeding = proceeding_state::none;
            if (service_platform != nullptr)
                stopping = stopping_sequence::set_service_state_stopped;
            else
                stopping = stopping_sequence::stop_console_platform;
            return proceed_result::continue_;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }

        case stopping_sequence::set_service_state_stopped :
        switch (proceeding)
        {
            case proceeding_state::none :
            proceeding = proceeding_state::stopping;
            service_platform->set_state_stopped ();
            return proceed_result::continue_;

            case proceeding_state::stopping : return proceed_result::nothing_to_do;

            case proceeding_state::stopped :
            proceeding = proceeding_state::none;
            stopping = stopping_sequence::stop_service_platform;
            return proceed_result::continue_;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }

        case stopping_sequence::stop_service_platform :
        switch (proceeding)
        {
            case proceeding_state::none :
            proceeding = proceeding_state::stopping;
            service_platform->stop ();
            return proceed_result::continue_;

            case proceeding_state::stopping : return proceed_result::nothing_to_do;

            case proceeding_state::stopped :
            proceeding = proceeding_state::none;
            stopping = stopping_sequence::exit_application;
            return proceed_result::continue_;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }

        case stopping_sequence::stop_console_platform :
        switch (proceeding)
        {
            case proceeding_state::none :
            proceeding = proceeding_state::stopping;
            console_platform->stop ();
            return proceed_result::continue_;

            case proceeding_state::stopping : return proceed_result::nothing_to_do;

            case proceeding_state::stopped :
            proceeding = proceeding_state::none;
            stopping = stopping_sequence::exit_application;
            return proceed_result::continue_;

            default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
        }

        case stopping_sequence::exit_application :
        // The exit code can be set anyway.
        // https://code.woboq.org/qt6/qtbase/src/corelib/kernel/qeventloop.cpp.html#_ZN10QEventLoop4exitEi
        if (not exiting_abruptly)
        {
            if (exit_code == 0)
                qCInfo (category, "Exit.");
            else
                qCInfo (category, "Exit with the result: '%d'.", exit_code);
            application->exit (exit_code);
        }
        stopping = stopping_sequence::set_state_stopped;
        [[ fallthrough ]];

        case stopping_sequence::set_state_stopped :
        state.state = service_state::stopped;
        state.target_state = target_service_state::none;
        qCInfo (category, "Stopped.");
        stopping = stopping_sequence::done;
        system_events.clear ();
        if (not this_->isSignalConnected (QMetaMethod::fromSignal (& service::state_changed)))
            return proceed_result::nothing_to_do;
        {
            const QPointer<const service> this_exists (this_);
            Q_EMIT this_->state_changed ();
            if (this_exists.isNull ())
                return proceed_result::destroyed;
        }
        return proceed_result::nothing_to_do;

        case stopping_sequence::done :
        default : Q_UNREACHABLE (); return proceed_result::nothing_to_do;
    }
}

proceed_result service_implementation::process_error ()
{
    service_error value (std::move (error_.value ()));
    error_ = std::nullopt;

    // Forward 'QMessageLogContext' from the location where the error was created?
    qCWarning (category).noquote () << value.text;

    // Filter out the error if it is not of interest in the current state or is inappropriate.
    if (state.target_state != target_service_state::serving)
        return proceed_result::continue_;
    const auto [ filtered, recoverable ] = [this] (const service_error & error)
    {
        struct result { bool filtered; bool recoverable; };

        switch (error.error)
        {
            case service_error::not_system_service :
            switch (starting)
            {
                case starting_sequence::set_up_service_platform :
                case starting_sequence::start_service_platform :
                return result { with_running_as_console_application, true };
                default : return result { true, false };
            }

            case service_error::failed_to_retrieve_configuration :
            switch (starting)
            {
                case starting_sequence::retrieve_configuration :
                return result { false, true };
                default : return result { true, false };
            }

            case service_error::failed_to_run :
            switch (starting)
            {
                case starting_sequence::set_up_service_platform :
                case starting_sequence::start_service_platform :
                case starting_sequence::retrieve_configuration :
                case starting_sequence::set_service_state_serving :
                case starting_sequence::set_up_console_platform :
                case starting_sequence::start_console_platform :
                return result { false, false };
                default : return result { true, false };
            }

            default : return result { true, false };
        }
    } (value);
    if (recoverable and filtered)
        return proceed_result::continue_;
    if (not filtered)
    {
        error = std::move (value);
        if (this_->isSignalConnected (QMetaMethod::fromSignal (& service::failed)))
        {
            error_ignored = false;
            processing_recoverable_error = recoverable; //and error.value ().recoverable ();
            const QPointer<const service> this_exists (this_);
            Q_EMIT this_->failed ();
            if (this_exists.isNull ())
                return proceed_result::destroyed;
            processing_recoverable_error = false;
            if (error_ignored)
            {
                error_ignored = false;
                error.reset ();
                qCInfo (category, "Ignoring the error.");
                return proceed_result::continue_;
            }
        }
    }
    state.target_state = target_service_state::stopped;
    return proceed_result::continue_;
}

// May add a user callback for flexibility.
void service_implementation::process_system_event ()
{
    const service_system_event event (std::move (system_events.front ()));
    system_events.pop_front ();
    switch (event.action)
    {
        case service_system_event::stop :
        state.target_state = target_service_state::stopped;
        qCInfo (category, "Stop on signal: '%s'.", qUtf8Printable (event.name));
        break;
    }
}

void service_implementation::set_up_application_controller ()
{
    const auto plugins (service_implementation::plugins<application_controller_plugin> ());
    application = [& plugins, this] ()
    {
        for (const auto & plugin : plugins)
        {
            if (plugin->metaObject () != & background_application_controller_plugin_qt::staticMetaObject)
            {
                auto * const result (plugin->create (this_));
                if (result != nullptr)
                    return result;
            }
        }
        return static_cast<application_controller *> (new application_controller_qt (this_));
    } ();
    QObject::connect (
        application, & application_controller::exiting,
        this_, std::bind (& service_implementation::shut_down_before_application_exits, this)
    );
}

// Do not prevent the user from exiting.
// The platform implementation will hold the event loop if required.
void service_implementation::shut_down_before_application_exits ()
{
    if (state.state == service_state::stopped or stopping >= stopping_sequence::exit_application)
        return;
    if (exiting_abruptly)
        return;
    exiting_abruptly = true;
    if (state.target_state != target_service_state::stopped)
    {
        state.target_state = target_service_state::stopped;
        proceed_from_event_loop ();
    }
    qCInfo (category, "The application exits unexpectedly.");
}

void service_implementation::process_system_event_received (const service_system_event & event)
{
    if (state.state == service_state::stopped or stopping >= stopping_sequence::exit_application)
        return;
    system_events.push_back (event);
    proceed_from_event_loop ();
}

void service_implementation::set_up_service_platform ()
{
    const auto plugins (
        [] ()
        {
            const auto plugins (service_implementation::plugins<service_platform_plugin> ());
            std::multimap<unsigned int, service_platform_plugin *> result;
            for (auto * const plugin : plugins)
                result.emplace (plugin->order (), plugin);
            return result;
        } ()
    );
    service_platform = [& plugins, this] ()
    {
        for (const auto & [ order, plugin ] : plugins)
        {
            if (not plugin->detect ())
                continue;
            auto * const result (plugin->create (this_));
            if (result != nullptr)
                return result;
        }
        return static_cast<class service_platform *> (nullptr);
    } ();
    if (service_platform == nullptr)
    {
        proceeding = proceeding_state::failed;
        error_ = service_error
        { // c++20 designated initializers
            /*.error =*/service_error::failed_to_run,
            /*.text =*/QStringLiteral ("Failed to run as a service. There is no implementation suitable for the platform.")
        };
        return;
    }
    if (not service_platform->check ())
    {
        proceeding = proceeding_state::failed;
        error_ = service_error
        { // c++20 designated initializers
            /*.error =*/service_error::not_system_service,
            /*.text =*/QStringLiteral ("Failed to run as a service. This process is not a service spawned by the system.")
        };
        return;
    }
    QObject::connect (
        service_platform, & service_platform::started,
        this_, std::bind (& service_implementation::process_service_platform_started, this)
    );
    QObject::connect (
        service_platform, & service_platform::failed_to_start,
        this_, std::bind (& service_implementation::process_service_platform_failed_to_start, this, std::placeholders::_1)
    );
    QObject::connect (
        service_platform, & service_platform::stopped,
        this_, std::bind (& service_implementation::process_service_platform_stopped, this)
    );
    QObject::connect (
        service_platform, & service_platform::state_serving_set,
        this_, std::bind (& service_implementation::process_service_state_serving_set, this)
    );
    QObject::connect (
        service_platform, & service_platform::failed_to_set_state_serving,
        this_, std::bind (& service_implementation::process_failed_to_set_service_state_serving, this, std::placeholders::_1)
    );
    QObject::connect (
        service_platform, & service_platform::state_stopping_set,
        this_, std::bind (& service_implementation::process_service_state_stopping_set, this)
    );
    QObject::connect (
        service_platform, & service_platform::state_stopped_set,
        this_, std::bind (& service_implementation::process_service_state_stopped_set, this)
    );
    QObject::connect (
        service_platform, & service_platform::configuration_retrieved,
        this_, std::bind (& service_implementation::process_service_configuration_retrieved, this, std::placeholders::_1)
    );
    QObject::connect (
        service_platform, & service_platform::failed_to_retrieve_configuration,
        this_, std::bind (& service_implementation::process_failed_to_retrieve_service_configuration, this, std::placeholders::_1)
    );
    QObject::connect (
        service_platform, & service_platform::event_received,
        this_, std::bind (& service_implementation::process_system_event_received, this, std::placeholders::_1)
    );
    proceeding = proceeding_state::started;
}

void service_implementation::process_service_platform_started ()
{
    if (starting != starting_sequence::start_service_platform or proceeding != proceeding_state::starting)
        return;
    proceeding = proceeding_state::started;
    proceed_from_event_loop ();
}

void service_implementation::process_service_platform_failed_to_start (const service_error & error)
{
    if (starting != starting_sequence::start_service_platform or proceeding != proceeding_state::starting)
        return;
    proceeding = proceeding_state::failed;
    error_ = error;
    proceed_from_event_loop ();
}

void service_implementation::process_service_platform_stopped ()
{
    if (stopping != stopping_sequence::stop_service_platform or proceeding != proceeding_state::stopping)
        return;
    proceeding = proceeding_state::stopped;
    proceed_from_event_loop ();
}

void service_implementation::process_service_state_serving_set ()
{
    if (starting != starting_sequence::set_service_state_serving or proceeding != proceeding_state::starting)
        return;
    proceeding = proceeding_state::started;
    proceed_from_event_loop ();
}

void service_implementation::process_failed_to_set_service_state_serving (const service_error & error)
{
    if (starting != starting_sequence::set_service_state_serving or proceeding != proceeding_state::starting)
        return;
    proceeding = proceeding_state::failed;
    error_ = error;
    proceed_from_event_loop ();
}

void service_implementation::process_service_state_stopping_set ()
{
    if (stopping != stopping_sequence::set_service_state_stopping or proceeding != proceeding_state::stopping)
        return;
    proceeding = proceeding_state::stopped;
    proceed_from_event_loop ();
}

void service_implementation::process_service_state_stopped_set ()
{
    if (stopping != stopping_sequence::set_service_state_stopped or proceeding != proceeding_state::stopping)
        return;
    proceeding = proceeding_state::stopped;
    proceed_from_event_loop ();
}

void service_implementation::process_service_configuration_retrieved (const service_configuration & configuration)
{
    if (starting != starting_sequence::retrieve_configuration or proceeding != proceeding_state::starting)
        return;
    proceeding = proceeding_state::started;
    service_implementation::configuration = configuration;
    proceed_from_event_loop ();
}

void service_implementation::process_failed_to_retrieve_service_configuration (const service_error & error)
{
    if (starting != starting_sequence::retrieve_configuration or proceeding != proceeding_state::starting)
        return;
    proceeding = proceeding_state::failed;
    error_ = error;
    proceed_from_event_loop ();
}

void service_implementation::set_up_console_platform ()
{
    const auto plugins (
        [] ()
        {
            const auto plugins (service_implementation::plugins<console_platform_plugin> ());
            std::multimap<unsigned int, console_platform_plugin *> result;
            for (auto * const plugin : plugins)
                result.emplace (plugin->order (), plugin);
            return result;
        } ()
    );
    console_platform = [& plugins, this] ()
    {
        for (const auto & [ order, plugin ] : plugins)
        {
            auto * const result (plugin->create (this_));
            if (result != nullptr)
                return result;
        }
        return static_cast<class console_platform *> (nullptr);
    } ();
    if (console_platform == nullptr)
    {
        proceeding = proceeding_state::failed;
        error_ = service_error
        { // c++20 designated initializers
            /*.error =*/service_error::failed_to_run,
            /*.text =*/QStringLiteral ("Failed to run as a console application. There is no implementation suitable for the platform.")
        };
        return;
    }
    QObject::connect (
        console_platform, & console_platform::started,
        this_, std::bind (& service_implementation::process_console_platform_started, this)
    );
    QObject::connect (
        console_platform, & console_platform::failed_to_start,
        this_, std::bind (& service_implementation::process_console_platform_failed_to_start, this, std::placeholders::_1)
    );
    QObject::connect (
        console_platform, & console_platform::stopped,
        this_, std::bind (& service_implementation::process_console_platform_stopped, this)
    );
    QObject::connect (
        console_platform, & console_platform::event_received,
        this_, std::bind (& service_implementation::process_system_event_received, this, std::placeholders::_1)
    );
    proceeding = proceeding_state::started;
}

void service_implementation::process_console_platform_started ()
{
    if (starting != starting_sequence::start_console_platform or proceeding != proceeding_state::starting)
        return;
    proceeding = proceeding_state::started;
    proceed_from_event_loop ();
}

void service_implementation::process_console_platform_failed_to_start (const service_error & error)
{
    if (starting != starting_sequence::start_console_platform or proceeding != proceeding_state::starting)
        return;
    proceeding = proceeding_state::failed;
    error_ = error;
    proceed_from_event_loop ();
}

void service_implementation::process_console_platform_stopped ()
{
    if (stopping != stopping_sequence::stop_console_platform or proceeding != proceeding_state::stopping)
        return;
    proceeding = proceeding_state::stopped;
    proceed_from_event_loop ();
}

template <class T> std::vector<T *> service_implementation::plugins ()
{
    const auto interface_id (QString::fromUtf8 (qobject_interface_iid<T *> ()));
    std::vector<T *> result;
    const auto plugins (QPluginLoader::staticPlugins ());
    for (const auto & plugin : plugins)
    {
        if (plugin.metaData ().value (QStringLiteral ("IID")).toString () != interface_id)
            continue;
        QObject * const instance_ (plugin.instance ());
        if (instance_ == nullptr)
            continue;
        auto * const instance (qobject_cast<T *> (instance_));
        if (instance == nullptr)
            continue;
        result.push_back (instance);
    }
    return result;
}

} // namespace background
