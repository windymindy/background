#include <vector>

//#include <QtTest/QtTest>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#if not defined QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif
#include <QtCore/QPluginLoader>

#include <background/service>
#include <background/background_application_controller.hpp>
#include <background/background_service_platform.hpp>
#include <background/background_console_platform.hpp>

using namespace background;

class test_service : public QObject
{
    private Q_SLOTS:
    void setting_failed_to_start_shuts_down ();
    void setting_started_while_stopping_ignored ();
    void stop_not_emitted_until_set_started_1 ();
    void stop_not_emitted_until_set_started_2 ();
    void setting_with_stop_starting_stops_while_starting ();

    void setting_with_running_as_console_application_disables_error_1 ();
    void setting_with_running_as_console_application_disables_error_2 ();
    void ignoring_not_system_service_error_runs_as_console_application_1 ();
    void ignoring_not_system_service_error_runs_as_console_application_2 ();

    void setting_no_running_as_service_runs_as_console_application ();

    void setting_no_retrieving_configuration_skips_retrieving_configuration ();
    void ignoring_failed_to_retrieve_configuration_error_runs ();

    void failing_to_start_platform_shuts_down_1 ();
    void failing_to_start_platform_shuts_down_2 ();
    void failing_to_set_state_serving_shuts_down ();

    void exiting_application_shuts_down ();

    void shutting_down_in_initial_state_exits_1 ();
    void shutting_down_in_initial_state_exits_2 ();
    void shutting_down_in_initial_state_exits_3 ();

    void reentering_event_loop_does_not_lock_lifecycle ();
    void receiving_event_while_proceeding_reenters ();

    void system_events_logged_while_stopping ();

    void destroying_incorrectly_does_not_crash_1 ();

    private:
    Q_OBJECT
};

class application_controller_test : public application_controller
{
    public :
    application_controller_test ();
    ~application_controller_test ();

    QSignalSpy exited;

    public Q_SLOTS :
    void exit (int exit_code) override;

    Q_SIGNALS :
    void exit_ (int exit_code);

    private :
    Q_OBJECT
};

class service_platform_test : public service_platform
{
    public :
    service_platform_test ();
    ~service_platform_test ();

    QSignalSpy checked;

    QSignalSpy started_;
    QSignalSpy stopped_;

    QSignalSpy state_serving_set_;
    QSignalSpy state_stopping_set_;
    QSignalSpy state_stopped_set_;

    QSignalSpy configuration_retrieved_;

    public Q_SLOTS :
    bool check () override;

    void start () override;
    void stop () override;

    void set_state_serving () override;
    void set_state_stopping () override;
    void set_state_stopped () override;

    void retrieve_configuration () override;

    Q_SIGNALS :
    bool check_ ();

    void start_ ();
    void stop_ ();

    void set_state_serving_ ();
    void set_state_stopping_ ();
    void set_state_stopped_ ();

    void retrieve_configuration_ ();

    public Q_SLOTS :
    void send_stop ();

    private :
    Q_OBJECT
};

class console_platform_test : public console_platform
{
    public :
    console_platform_test ();
    ~console_platform_test ();

    QSignalSpy started_;
    QSignalSpy stopped_;

    public Q_SLOTS :
    void start () override;
    void stop () override;

    Q_SIGNALS :
    void start_ ();
    void stop_ ();

    public Q_SLOTS :
    void send_stop ();

    private :
    Q_OBJECT
};

struct serving_state_changes
{
    serving_state_changes (service * service);

    bool wait (service_state value);

    service * const service_;
    std::vector<serving_state> changes;
    std::optional<QSignalSpy> changed;

    static std::vector<serving_state> none_to_stopped ();
    static std::vector<serving_state> serving_to_stopped ();
    static std::vector<serving_state> serving_stopping_to_stopped ();
};

struct signal_utility : QObject
{
    static bool connected (const QObject * object, const QSignalSpy & signal);
};

void test_service::setting_failed_to_start_shuts_down ()
{
    application_controller_test application;
    service_platform_test service_;
    service service;
    QSignalSpy start (& service, & service::start);
    QSignalSpy stop (& service, & service::stop);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    connect (& service, & service::stop, & service, & service::set_stopped);
    service.run ();

    QVERIFY (start.wait ());
    service.set_failed_to_start ();

    QVERIFY (state_changed.wait (service_state::stopped));
    QVERIFY (not stop.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
    QVERIFY (failed.isEmpty ());
}

void test_service::setting_started_while_stopping_ignored ()
{
    application_controller_test application;
    service_platform_test service_;
    service service;
    QSignalSpy start (& service, & service::start);
    QSignalSpy stop (& service, & service::stop);
    serving_state_changes state_changed (& service);

    service.set_with_stop_starting ().run ();

    QVERIFY (start.wait ());
    service_.send_stop ();

    QVERIFY (stop.wait ());
    service.set_started ();
    QCoreApplication::processEvents ();
    QCoreApplication::processEvents ();
    QCoreApplication::processEvents ();
    QCOMPARE (service.state ().state, service_state::stopping);
    service.set_stopped ();

    QVERIFY (state_changed.wait (service_state::stopped));
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
}

void test_service::stop_not_emitted_until_set_started_1 ()
{
    application_controller_test application;
    service_platform_test service_;
    service service;
    QSignalSpy start (& service, & service::start);
    QSignalSpy stop (& service, & service::stop);
    QSignalSpy state_changed (& service, & service::state_changed);

    connect (& service, & service::stop, & service, & service::set_stopped);
    service.run ();

    QVERIFY (start.wait ());

    service_.send_stop ();
    QCoreApplication::processEvents ();
    QCoreApplication::processEvents ();
    QCoreApplication::processEvents ();
    QVERIFY (stop.isEmpty ());

    service.set_started ();
    QVERIFY (state_changed.wait ());
    QVERIFY (not stop.isEmpty ());
}

void test_service::stop_not_emitted_until_set_started_2 ()
{
    application_controller_test application;
    service_platform_test service_;
    service service;
    QSignalSpy start (& service, & service::start);
    QSignalSpy stop (& service, & service::stop);
    QSignalSpy state_changed (& service, & service::state_changed);

    connect (& service, & service::stop, & service, & service::set_stopped);
    service.run ();

    QVERIFY (start.wait ());

    service.shut_down ();
    QCoreApplication::processEvents ();
    QCoreApplication::processEvents ();
    QCoreApplication::processEvents ();
    QVERIFY (stop.isEmpty ());

    service.set_started ();
    QVERIFY (state_changed.wait ());
    QVERIFY (not stop.isEmpty ());
}

void test_service::setting_with_stop_starting_stops_while_starting ()
{
    application_controller_test application;
    service_platform_test service_;
    service service;
    QSignalSpy start (& service, & service::start);
    QSignalSpy stop (& service, & service::stop);
    QSignalSpy state_changed (& service, & service::state_changed);

    service.set_with_stop_starting ().run ();

    QVERIFY (start.wait ());

    service_.send_stop ();
    QVERIFY (stop.wait ());

    service.set_stopped ();
    QVERIFY (state_changed.wait ());
}

void test_service::setting_with_running_as_console_application_disables_error_1 ()
{
    application_controller_test application;
    service_platform_test service_;
    console_platform_test console;
    service service;
    QSignalSpy start (& service, & service::start);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    connect (& service, & service::start, & service, & service::set_started);
    connect (& service, & service::stop, & service, & service::set_stopped);
    connect (
        & service,
        & service::state_changed,
        & service,
        [& service, & console] ()
        {
            if (not service.state ().serving ())
                return;
            console.send_stop ();
        }
    );
    service.set_with_running_as_console_application ().run ();

    connect (
        & service_, & service_platform_test::check_,
        & service_, [] () { return false; }
    );

    state_changed.wait (service_state::stopped);
    QVERIFY (not start.isEmpty ());
    QVERIFY (service.running_as_service ().has_value ());
    QCOMPARE (service.running_as_service ().value (), false);
    QVERIFY (not service.configuration ().has_value ());
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
    QVERIFY (failed.isEmpty ());
}

void test_service::setting_with_running_as_console_application_disables_error_2 ()
{
    application_controller_test application;
    service_platform_test service_;
    console_platform_test console;
    service service;
    QSignalSpy start (& service, & service::start);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    connect (& service, & service::start, & service, & service::set_started);
    connect (& service, & service::stop, & service, & service::set_stopped);
    connect (
        & service,
        & service::state_changed,
        & service,
        [& service, & console] ()
        {
            if (not service.state ().serving ())
                return;
            console.send_stop ();
        }
    );
    service.set_with_running_as_console_application ().run ();

    connect (
        & service_,
        & service_platform_test::start_,
        & service_,
        [& service_] ()
        {
            Q_EMIT service_.failed_to_start (
                service_error
                {
                    service_error::not_system_service,
                    QStringLiteral ("Failed to start. Emulating not a service error.")
                }
            );
        }
    );

    state_changed.wait (service_state::stopped);
    QVERIFY (not start.isEmpty ());
    QVERIFY (service.running_as_service ().has_value ());
    QCOMPARE (service.running_as_service ().value (), false);
    QVERIFY (not service.configuration ().has_value ());
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
    QVERIFY (failed.isEmpty ());
}

void test_service::ignoring_not_system_service_error_runs_as_console_application_1 ()
{
    application_controller_test application;
    service_platform_test service_;
    console_platform_test console;
    service service;
    QSignalSpy start (& service, & service::start);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    connect (& service, & service::start, & service, & service::set_started);
    connect (& service, & service::stop, & service, & service::set_stopped);
    connect (
        & service,
        & service::state_changed,
        & service,
        [& service, & console] ()
        {
            if (not service.state ().serving ())
                return;
            console.send_stop ();
        }
    );
    service.run ();

    connect (
        & service_, & service_platform_test::check_,
        & service_, [] () { return false; }
    );
    connect (& service, & service::failed, & service, & service::ignore_error);

    QVERIFY (start.wait ());
    QVERIFY (service.running_as_service ().has_value ());
    QCOMPARE (service.running_as_service ().value (), false);
    QVERIFY (not service.configuration ().has_value ());
    QVERIFY (not service.error ().has_value ());
    QVERIFY (not failed.isEmpty ());

    state_changed.wait (service_state::stopped);
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
}

void test_service::ignoring_not_system_service_error_runs_as_console_application_2 ()
{
    application_controller_test application;
    service_platform_test service_;
    console_platform_test console;
    service service;
    QSignalSpy start (& service, & service::start);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    connect (& service, & service::start, & service, & service::set_started);
    connect (& service, & service::stop, & service, & service::set_stopped);
    connect (
        & service,
        & service::state_changed,
        & service,
        [& service, & console] ()
        {
            if (not service.state ().serving ())
                return;
            console.send_stop ();
        }
    );
    service.run ();

    connect (
        & service_,
        & service_platform_test::start_,
        & service_,
        [& service_] ()
        {
            Q_EMIT service_.failed_to_start (
                service_error
                {
                    service_error::not_system_service,
                    QStringLiteral ("Failed to start. Emulating not a service error.")
                }
            );
        }
    );
    connect (& service, & service::failed, & service, & service::ignore_error);

    QVERIFY (start.wait ());
    QVERIFY (service.running_as_service ().has_value ());
    QCOMPARE (service.running_as_service ().value (), false);
    QVERIFY (not service.configuration ().has_value ());
    QVERIFY (not service.error ().has_value ());
    QVERIFY (not failed.isEmpty ());

    state_changed.wait (service_state::stopped);
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
}

void test_service::setting_no_running_as_service_runs_as_console_application ()
{
    application_controller_test application;
    service_platform_test service_;
    console_platform_test console;
    service service;
    QSignalSpy start (& service, & service::start);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    connect (& service, & service::start, & service, & service::set_started);
    connect (& service, & service::stop, & service, & service::set_stopped);
    connect (& service, & service::start, & service, & service::shut_down);
    service.set_no_running_as_service ().run ();

    QVERIFY (start.wait ());
    QVERIFY (service.running_as_service ().has_value ());
    QCOMPARE (service.running_as_service ().value (), false);
    QVERIFY (not service.configuration ().has_value ());
    QVERIFY (not service.error ().has_value ());
    QVERIFY (failed.isEmpty ());

    state_changed.wait (service_state::stopped);
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());

    QVERIFY (service_.checked.isEmpty ());
    QVERIFY (service_.started_.isEmpty ());
    QVERIFY (service_.stopped_.isEmpty ());
}

void test_service::setting_no_retrieving_configuration_skips_retrieving_configuration ()
{
    application_controller_test application;
    service_platform_test service_;
    service service;
    QSignalSpy start (& service, & service::start);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    connect (& service, & service::start, & service, & service::set_started);
    connect (& service, & service::stop, & service, & service::set_stopped);
    connect (
        & service,
        & service::state_changed,
        & service,
        [& service, & service_] ()
        {
            if (not service.state ().serving ())
                return;
            service_.send_stop ();
        }
    );
    service.set_no_retrieving_configuration ().run ();

    state_changed.wait (service_state::stopped);
    QVERIFY (not start.isEmpty ());
    QVERIFY (service.running_as_service ().has_value ());
    QCOMPARE (service.running_as_service ().value (), true);
    QVERIFY (not service.configuration ().has_value ());
    QVERIFY (service_.configuration_retrieved_.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
    QVERIFY (failed.isEmpty ());
}

void test_service::ignoring_failed_to_retrieve_configuration_error_runs ()
{
    application_controller_test application;
    service_platform_test service_;
    service service;
    QSignalSpy start (& service, & service::start);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    connect (& service, & service::start, & service, & service::set_started);
    connect (& service, & service::stop, & service, & service::set_stopped);
    connect (
        & service,
        & service::state_changed,
        & service,
        [& service, & service_] ()
        {
            if (not service.state ().serving ())
                return;
            service_.send_stop ();
        }
    );
    service.run ();

    connect (
        & service_,
        & service_platform_test::retrieve_configuration_,
        & service_,
        [& service_] ()
        {
            Q_EMIT service_.failed_to_retrieve_configuration (
                service_error
                {
                    service_error::failed_to_retrieve_configuration,
                    QStringLiteral ("Failed to retrieve service configuration. Emulating failed to retrieve configuration error.")
                }
            );
        }
    );
    connect (& service, & service::failed, & service, & service::ignore_error);

    state_changed.wait (service_state::stopped);
    QVERIFY (not start.isEmpty ());
    QVERIFY (service.running_as_service ().has_value ());
    QCOMPARE (service.running_as_service ().value (), true);
    QVERIFY (not service.configuration ().has_value ());
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
    QVERIFY (not failed.isEmpty ());
}

void test_service::failing_to_start_platform_shuts_down_1 ()
{
    application_controller_test application;
    service_platform_test service_;
    service service;
    QSignalSpy start (& service, & service::start);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    service.run ();

    connect (
        & service_,
        & service_platform_test::start_,
        & service_,
        [& service_] ()
        {
            Q_EMIT service_.failed_to_start (
                service_error
                {
                    service_error::failed_to_run,
                    QStringLiteral ("Failed to start. Emulating failed to start error.")
                }
            );
        }
    );

    state_changed.wait (service_state::stopped);
    QVERIFY (start.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
    QVERIFY (not failed.isEmpty ());
}

void test_service::failing_to_start_platform_shuts_down_2 ()
{
    application_controller_test application;
    console_platform_test console;
    service service;
    QSignalSpy start (& service, & service::start);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    service.set_no_running_as_service ().run ();

    connect (
        & console,
        & console_platform_test::start_,
        & console,
        [& console] ()
        {
            Q_EMIT console.failed_to_start (
                service_error
                {
                    service_error::failed_to_run,
                    QStringLiteral ("Failed to start. Emulating failed to start error.")
                }
            );
        }
    );

    state_changed.wait (service_state::stopped);
    QVERIFY (start.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
    QVERIFY (not failed.isEmpty ());
}

void test_service::failing_to_set_state_serving_shuts_down ()
{
    application_controller_test application;
    service_platform_test service_;
    service service;
    QSignalSpy start (& service, & service::start);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    connect (& service, & service::start, & service, & service::set_started);
    connect (& service, & service::stop, & service, & service::set_stopped);
    service.run ();

    connect (
        & service_,
        & service_platform_test::set_state_serving_,
        & service_,
        [& service_] ()
        {
            Q_EMIT service_.failed_to_set_state_serving (
                service_error
                {
                    service_error::failed_to_run,
                    QStringLiteral ("Failed to start. Emulating failed to set state serving error.")
                }
            );
        }
    );

    state_changed.wait (service_state::stopped);
    QVERIFY (not start.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
    QVERIFY (not failed.isEmpty ());
}

void test_service::exiting_application_shuts_down ()
{
    application_controller_test application;
    service_platform_test service_;
    service service;
    serving_state_changes state_changed (& service);

    connect (& service, & service::start, & service, & service::set_started);
    connect (& service, & service::stop, & service, & service::set_stopped);
    service.run ();

    QVERIFY (state_changed.wait (service_state::serving));
    Q_EMIT application.exiting ();

    QVERIFY (state_changed.wait (service_state::stopped));
    QVERIFY (application.exited.isEmpty ());
}

void test_service::shutting_down_in_initial_state_exits_1 ()
{
    application_controller_test application;
    service service;
    QSignalSpy start (& service, & service::start);
    QSignalSpy stop (& service, & service::stop);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    service.shut_down ();

    state_changed.wait (service_state::stopped);
    QVERIFY (start.isEmpty ());
    QVERIFY (stop.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
    QVERIFY (failed.isEmpty ());
    QCOMPARE (application.exited.count (), 1);
    QCOMPARE (application.exited.at (0).at (0), 0);
}

void test_service::shutting_down_in_initial_state_exits_2 ()
{
    #if not defined NDEBUG
    QSKIP ("Expected to abort.");
    #endif

    application_controller_test application;
    service service;
    QSignalSpy start (& service, & service::start);
    QSignalSpy stop (& service, & service::stop);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    service.shut_down ();
    service.run (); // Should cause no effect.

    state_changed.wait (service_state::stopped);
    QVERIFY (start.isEmpty ());
    QVERIFY (stop.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
    QVERIFY (failed.isEmpty ());
    QCOMPARE (application.exited.count (), 1);
    QCOMPARE (application.exited.at (0).at (0), 0);
}

void test_service::shutting_down_in_initial_state_exits_3 ()
{
    application_controller_test application;
    service service;
    QSignalSpy start (& service, & service::start);
    QSignalSpy stop (& service, & service::stop);
    serving_state_changes state_changed (& service);
    QSignalSpy failed (& service, & service::failed);

    service.run ();
    service.shut_down ();

    state_changed.wait (service_state::stopped);
    QVERIFY (start.isEmpty ());
    QVERIFY (stop.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
    QVERIFY (failed.isEmpty ());
    QCOMPARE (application.exited.count (), 1);
    QCOMPARE (application.exited.at (0).at (0), 0);
}

void test_service::reentering_event_loop_does_not_lock_lifecycle ()
{
    application_controller_test application;
    service_platform_test service_;
    service service;
    QSignalSpy start (& service, & service::start);
    QSignalSpy stop (& service, & service::stop);
    serving_state_changes state_changed (& service);

    service.run ();

    connect (
        & service,
        & service::start,
        & service,
        [& service] ()
        {
            service.set_started ();
            do
                QCoreApplication::processEvents (QEventLoop::AllEvents bitor QEventLoop::WaitForMoreEvents);
            while (not service.state ().stopped ());
        }
    );
    connect (
        & service,
        & service::stop,
        & service,
        [& service] ()
        {
            service.set_stopped ();
            do
                QCoreApplication::processEvents (QEventLoop::AllEvents bitor QEventLoop::WaitForMoreEvents);
            while (not service.state ().stopped ());
        }
    );
    connect (
        & service,
        & service::state_changed,
        & service,
        [& service, & service_] ()
        {
            if (not service.state ().serving ())
                return;
            service_.send_stop ();
            do
                QCoreApplication::processEvents (QEventLoop::AllEvents bitor QEventLoop::WaitForMoreEvents);
            while (not service.state ().stopped ());
        }
    );

    QVERIFY (state_changed.wait (service_state::stopped));
    QVERIFY (not start.isEmpty ());
    QVERIFY (not stop.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
}

void test_service::receiving_event_while_proceeding_reenters ()
{
    application_controller_test application;
    service_platform_test service_;
    service service;
    serving_state_changes state_changed (& service);

    connect (& service, & service::start, & service, & service::set_started);
    connect (& service, & service::stop, & service, & service::set_stopped);
    service.run ();

    static QtMessageHandler log (nullptr);
    static service_platform_test * service_platform_ (nullptr);
    service_platform_ = & service_;
    // The easiest way to squeeze into 'proceed ()' to get control.
    log = qInstallMessageHandler (
        [] (const QtMsgType type, const QMessageLogContext & context, const QString & message)
        {
            log (type, context, message);

            if (message != QStringLiteral ("Serving..."))
                return;
            // 'shut_down ()' or anything that calls 'proceed_from_event_loop ()' internally must be processed.
            service_platform_->send_stop ();
        }
    );

    QVERIFY (state_changed.wait (service_state::stopped));
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());

    qInstallMessageHandler (log);
    service_platform_ = nullptr;
}

void test_service::system_events_logged_while_stopping ()
{
    application_controller_test application;
    console_platform_test console;
    service service;
    serving_state_changes state_changed (& service);

    connect (& service, & service::start, & service, & service::set_started);
    connect (& service, & service::stop, & service, & service::set_stopped);
    connect (
        & service,
        & service::state_changed,
        & service,
        [& service, & console] ()
        {
            if (not service.state ().serving ())
                return;
            console.send_stop ();
        }
    );
    service.set_no_running_as_service ().run ();

    connect (
        & console, & console_platform_test::stop_,
        & console, [] () {}
    );

    static QtMessageHandler log (nullptr);
    static console_platform_test * console_ (nullptr);
    console_ = & console;
    log = qInstallMessageHandler (
        [] (const QtMsgType type, const QMessageLogContext & context, const QString & message)
        {
            log (type, context, message);

            if (message != QStringLiteral ("Stop on signal: 'test 2'."))
                return;
            Q_EMIT console_->stopped ();
        }
    );

    QVERIFY (console.stopped_.wait ());
    QCOMPARE (service.state ().state, service_state::stopping);
    Q_EMIT console.event_received (
        service_system_event
        {
            service_system_event::stop,
            QStringLiteral ("test 2")
        }
    );

    QVERIFY (state_changed.wait (service_state::stopped));
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());

    qInstallMessageHandler (log);
    console_ = nullptr;
}

void test_service::destroying_incorrectly_does_not_crash_1 ()
{
    #if not defined NDEBUG
    QSKIP ("Expected to abort.");
    #endif

    application_controller_test application;
    service_platform_test service_;
    service * service (new class service);

    service->run ();

    connect (
        service,
        & service::start,
        service,
        [service] ()
        {
            delete service;
        }
    );
}

template <class T> T * plugin ()
{
    const auto plugins (QPluginLoader::staticPlugins ());
    for (const auto & plugin : plugins)
    {
        if (plugin.instance ()->metaObject () != & T::staticMetaObject)
            continue;
        return reinterpret_cast <T *> (plugin.instance ());
    }
    return nullptr;
}

class application_controller_plugin_test : public application_controller_plugin
{
    public :
    application_controller_plugin_test (QObject * parent = nullptr);

    public :
    application_controller * create (QObject * parent) override;

    private :
    application_controller * controller = nullptr;
    friend class application_controller_test;

    private :
    Q_OBJECT
    Q_PLUGIN_METADATA (IID "background.application_controller_plugin")
    Q_INTERFACES (background::application_controller_plugin)
};

Q_IMPORT_PLUGIN (application_controller_plugin_test)

application_controller_plugin_test::application_controller_plugin_test (QObject * const parent)
    : application_controller_plugin (parent)
{}

application_controller * application_controller_plugin_test::create (QObject * const parent)
{
    Q_UNUSED (parent)
    return controller;
}

application_controller_test::application_controller_test ()
    : application_controller (nullptr),
    exited (this, & application_controller_test::exit_)
{
    plugin<application_controller_plugin_test> ()->controller = this;
}

application_controller_test::~application_controller_test ()
{
    plugin<application_controller_plugin_test> ()->controller = nullptr;
}

void application_controller_test::exit (const int exit_code)
{
    Q_EMIT exit_ (exit_code);
}

class service_platform_plugin_test : public service_platform_plugin
{
    public :
    service_platform_plugin_test (QObject * parent = nullptr);

    public :
    unsigned int order () const override;
    bool detect () override;
    service_platform * create (QObject * parent) override;

    private :
    service_platform * service = nullptr;
    friend class service_platform_test;

    private :
    Q_OBJECT
    Q_PLUGIN_METADATA (IID "background.service_platform_plugin")
    Q_INTERFACES (background::service_platform_plugin)
};

Q_IMPORT_PLUGIN (service_platform_plugin_test)

service_platform_plugin_test::service_platform_plugin_test (QObject * const parent)
    : service_platform_plugin (parent)
{}

unsigned int service_platform_plugin_test::order () const
{
    return 1;
}

bool service_platform_plugin_test::detect ()
{
    return service != nullptr;
}

service_platform * service_platform_plugin_test::create (QObject * const parent)
{
    Q_UNUSED (parent)
    return service;
}

service_platform_test::service_platform_test ()
    : service_platform (nullptr),
    checked (this, & service_platform_test::check_),
    started_ (this, & service_platform_test::start_),
    stopped_ (this, & service_platform_test::stop_),
    state_serving_set_ (this, & service_platform_test::set_state_serving_),
    state_stopping_set_ (this, & service_platform_test::set_state_stopping_),
    state_stopped_set_ (this, & service_platform_test::set_state_stopped_),
    configuration_retrieved_ (this, & service_platform_test::retrieve_configuration_)
{
    plugin<service_platform_plugin_test> ()->service = this;
}

service_platform_test::~service_platform_test ()
{
    plugin<service_platform_plugin_test> ()->service = nullptr;
}

bool service_platform_test::check ()
{
    const bool result (Q_EMIT check_ ());
    if (not signal_utility::connected (this, checked))
        return true;
    return result;
}

void service_platform_test::start ()
{
    Q_EMIT start_ ();
    if (not signal_utility::connected (this, started_))
        Q_EMIT started ();
}

void service_platform_test::stop ()
{
    Q_EMIT stop_ ();
    if (not signal_utility::connected (this, stopped_))
        Q_EMIT stopped ();
}

void service_platform_test::set_state_serving ()
{
    Q_EMIT set_state_serving_ ();
    if (not signal_utility::connected (this, state_serving_set_))
        Q_EMIT state_serving_set ();
}

void service_platform_test::set_state_stopping ()
{
    Q_EMIT set_state_stopping_ ();
    if (not signal_utility::connected (this, state_stopping_set_))
        Q_EMIT state_stopping_set ();
}

void service_platform_test::set_state_stopped ()
{
    Q_EMIT set_state_stopped_ ();
    if (not signal_utility::connected (this, state_stopped_set_))
        Q_EMIT state_stopped_set ();
}

void service_platform_test::retrieve_configuration ()
{
    Q_EMIT retrieve_configuration_ ();
    if (not signal_utility::connected (this, configuration_retrieved_))
        Q_EMIT configuration_retrieved (
            service_configuration
            {
                QStringLiteral ("test_service"),
                QStringLiteral ("Test Service."),
                QStringLiteral ("test_service"),
                QStringLiteral ("test"),
            }
        );
}

void service_platform_test::send_stop ()
{
    Q_EMIT event_received (service_system_event { service_system_event::stop, QStringLiteral ("test") });
}

class console_platform_plugin_test : public console_platform_plugin
{
    public :
    console_platform_plugin_test (QObject * parent = nullptr);

    public :
    unsigned int order () const override;
    console_platform * create (QObject * parent) override;

    private :
    console_platform * console = nullptr;
    friend class console_platform_test;

    private :
    Q_OBJECT
    Q_PLUGIN_METADATA (IID "background.console_platform_plugin")
    Q_INTERFACES (background::console_platform_plugin)
};

Q_IMPORT_PLUGIN (console_platform_plugin_test)

console_platform_plugin_test::console_platform_plugin_test (QObject * const parent)
    : console_platform_plugin (parent)
{}

unsigned int console_platform_plugin_test::order () const
{
    return 1;
}

console_platform * console_platform_plugin_test::create (QObject * const parent)
{
    Q_UNUSED (parent)
    return console;
}

console_platform_test::console_platform_test ()
    : console_platform (nullptr),
    started_ (this, & console_platform_test::start_),
    stopped_ (this, & console_platform_test::stop_)
{
    plugin<console_platform_plugin_test> ()->console = this;
}

console_platform_test::~console_platform_test ()
{
    plugin<console_platform_plugin_test> ()->console = nullptr;
}

void console_platform_test::start ()
{
    Q_EMIT start_ ();
    if (not signal_utility::connected (this, started_))
        Q_EMIT started ();
}

void console_platform_test::stop ()
{
    Q_EMIT stop_ ();
    if (not signal_utility::connected (this, stopped_))
        Q_EMIT stopped ();
}

void console_platform_test::send_stop ()
{
    Q_EMIT event_received (service_system_event { service_system_event::stop, QStringLiteral ("test") });
}

serving_state_changes::serving_state_changes (service * const service)
    : changed (std::nullopt),
    service_ (service)
{
    QObject::connect (
        service,
        & service::state_changed,
        service,
        [this, service] ()
        {
            changes.push_back (service->state ());
        }
    );
}

bool serving_state_changes::wait (service_state value)
{
    if (not changed.has_value ())
        changed.emplace (service_, & service::state_changed);
    do
    {
        if (not changed.value ().wait ())
            return false;
    }
    while (changes.back ().state != value);
    return true;
}

std::vector<serving_state> serving_state_changes::none_to_stopped ()
{
    return { { service_state::stopped, target_service_state::none } };
}

std::vector<serving_state> serving_state_changes::serving_to_stopped ()
{
    return
    {
        { service_state::serving, target_service_state::none },
        { service_state::stopped, target_service_state::none }
    };
}

bool signal_utility::connected (const QObject * const object, const QSignalSpy & signal)
{
    return reinterpret_cast<const signal_utility *> (object)->receivers (signal.signal ().prepend ('2')) > 1;
}

QTEST_MAIN (test_service)

#include "test_service.moc"
