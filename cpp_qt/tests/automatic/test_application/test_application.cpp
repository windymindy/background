#include <vector>

//#include <QtTest/QtTest>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#if not defined QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif
#include <QtCore/QPluginLoader>

#include <background/application>
#include <background/background_event_loop_controller.hpp>
#include <background/background_service_platform.hpp>
#include <background/background_console_platform.hpp>

using namespace background;

class test_application : public QObject
{
    private Q_SLOTS:
    void setting_failed_to_start_shuts_down ();
    void setting_started_while_stopping_ignored ();
    void stop_not_emitted_until_set_started_1 ();
    void stop_not_emitted_until_set_started_2 ();
    void setting_with_stop_starting_stops_while_starting ();

    void setting_with_running_as_non_service_disables_error_1 ();
    void setting_with_running_as_non_service_disables_error_2 ();
    void ignoring_not_service_error_runs_as_console_application_1 ();
    void ignoring_not_service_error_runs_as_console_application_2 ();

    void setting_no_running_as_service_runs_as_console_application ();

    void setting_no_retrieving_configuration_skips_retrieving_configuration ();
    void ignoring_failed_to_retrieve_configuration_error_runs ();

    void setting_no_running_as_console_application_runs ();

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

class event_loop_controller_test : public event_loop_controller
{
    public :
    event_loop_controller_test ();
    ~event_loop_controller_test ();

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
    serving_state_changes (application * application);

    bool wait (service_state value);

    application * const application_;
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

void test_application::setting_failed_to_start_shuts_down ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    application application;
    QSignalSpy start (& application, & application::start);
    QSignalSpy stop (& application, & application::stop);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    connect (& application, & application::stop, & application, & application::set_stopped);
    application.run ();

    QVERIFY (start.wait ());
    application.set_failed_to_start ();

    QVERIFY (state_changed.wait (service_state::stopped));
    QVERIFY (not stop.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
    QVERIFY (failed.isEmpty ());
}

void test_application::setting_started_while_stopping_ignored ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    application application;
    QSignalSpy start (& application, & application::start);
    QSignalSpy stop (& application, & application::stop);
    serving_state_changes state_changed (& application);

    application.set_with_stop_starting ().run ();

    QVERIFY (start.wait ());
    service.send_stop ();

    QVERIFY (stop.wait ());
    application.set_started ();
    QCoreApplication::processEvents ();
    QCoreApplication::processEvents ();
    QCoreApplication::processEvents ();
    QCOMPARE (application.state ().state, service_state::stopping);
    application.set_stopped ();

    QVERIFY (state_changed.wait (service_state::stopped));
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
}

void test_application::stop_not_emitted_until_set_started_1 ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    application application;
    QSignalSpy start (& application, & application::start);
    QSignalSpy stop (& application, & application::stop);
    QSignalSpy state_changed (& application, & application::state_changed);

    connect (& application, & application::stop, & application, & application::set_stopped);
    application.run ();

    QVERIFY (start.wait ());

    service.send_stop ();
    QCoreApplication::processEvents ();
    QCoreApplication::processEvents ();
    QCoreApplication::processEvents ();
    QVERIFY (stop.isEmpty ());

    application.set_started ();
    QVERIFY (state_changed.wait ());
    QVERIFY (not stop.isEmpty ());
}

void test_application::stop_not_emitted_until_set_started_2 ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    application application;
    QSignalSpy start (& application, & application::start);
    QSignalSpy stop (& application, & application::stop);
    QSignalSpy state_changed (& application, & application::state_changed);

    connect (& application, & application::stop, & application, & application::set_stopped);
    application.run ();

    QVERIFY (start.wait ());

    application.shut_down ();
    QCoreApplication::processEvents ();
    QCoreApplication::processEvents ();
    QCoreApplication::processEvents ();
    QVERIFY (stop.isEmpty ());

    application.set_started ();
    QVERIFY (state_changed.wait ());
    QVERIFY (not stop.isEmpty ());
}

void test_application::setting_with_stop_starting_stops_while_starting ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    application application;
    QSignalSpy start (& application, & application::start);
    QSignalSpy stop (& application, & application::stop);
    QSignalSpy state_changed (& application, & application::state_changed);

    application.set_with_stop_starting ().run ();

    QVERIFY (start.wait ());

    service.send_stop ();
    QVERIFY (stop.wait ());

    application.set_stopped ();
    QVERIFY (state_changed.wait ());
}

void test_application::setting_with_running_as_non_service_disables_error_1 ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    console_platform_test console;
    application application;
    QSignalSpy start (& application, & application::start);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    connect (& application, & application::start, & application, & application::set_started);
    connect (& application, & application::stop, & application, & application::set_stopped);
    connect (
        & application,
        & application::state_changed,
        & application,
        [& application, & console] ()
        {
            if (not application.state ().serving ())
                return;
            console.send_stop ();
        }
    );
    application.set_with_running_as_non_service ().run ();

    connect (
        & service, & service_platform_test::check_,
        & service, [] () { return false; }
    );

    state_changed.wait (service_state::stopped);
    QVERIFY (not start.isEmpty ());
    QVERIFY (application.running_as_service ().has_value ());
    QCOMPARE (application.running_as_service ().value (), false);
    QVERIFY (not application.service_configuration ().has_value ());
    QVERIFY (application.running_as_console_application ().has_value ());
    QCOMPARE (application.running_as_console_application ().value (), true);
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
    QVERIFY (failed.isEmpty ());
}

void test_application::setting_with_running_as_non_service_disables_error_2 ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    console_platform_test console;
    application application;
    QSignalSpy start (& application, & application::start);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    connect (& application, & application::start, & application, & application::set_started);
    connect (& application, & application::stop, & application, & application::set_stopped);
    connect (
        & application,
        & application::state_changed,
        & application,
        [& application, & console] ()
        {
            if (not application.state ().serving ())
                return;
            console.send_stop ();
        }
    );
    application.set_with_running_as_non_service ().run ();

    connect (
        & service,
        & service_platform_test::start_,
        & service,
        [& service] ()
        {
            Q_EMIT service.failed_to_start (
                application_error
                {
                    application_error::not_service,
                    QStringLiteral ("Failed to start. Emulating not a service error.")
                }
            );
        }
    );

    state_changed.wait (service_state::stopped);
    QVERIFY (not start.isEmpty ());
    QVERIFY (application.running_as_service ().has_value ());
    QCOMPARE (application.running_as_service ().value (), false);
    QVERIFY (not application.service_configuration ().has_value ());
    QVERIFY (application.running_as_console_application ().has_value ());
    QCOMPARE (application.running_as_console_application ().value (), true);
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
    QVERIFY (failed.isEmpty ());
}

void test_application::ignoring_not_service_error_runs_as_console_application_1 ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    console_platform_test console;
    application application;
    QSignalSpy start (& application, & application::start);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    connect (& application, & application::start, & application, & application::set_started);
    connect (& application, & application::stop, & application, & application::set_stopped);
    connect (
        & application,
        & application::state_changed,
        & application,
        [& application, & console] ()
        {
            if (not application.state ().serving ())
                return;
            console.send_stop ();
        }
    );
    application.run ();

    connect (
        & service, & service_platform_test::check_,
        & service, [] () { return false; }
    );
    connect (& application, & application::failed, & application, & application::ignore_error);

    QVERIFY (start.wait ());
    QVERIFY (application.running_as_service ().has_value ());
    QCOMPARE (application.running_as_service ().value (), false);
    QVERIFY (not application.service_configuration ().has_value ());
    QVERIFY (application.running_as_console_application ().has_value ());
    QCOMPARE (application.running_as_console_application ().value (), true);
    QVERIFY (not application.error ().has_value ());
    QVERIFY (not failed.isEmpty ());

    state_changed.wait (service_state::stopped);
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
}

void test_application::ignoring_not_service_error_runs_as_console_application_2 ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    console_platform_test console;
    application application;
    QSignalSpy start (& application, & application::start);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    connect (& application, & application::start, & application, & application::set_started);
    connect (& application, & application::stop, & application, & application::set_stopped);
    connect (
        & application,
        & application::state_changed,
        & application,
        [& application, & console] ()
        {
            if (not application.state ().serving ())
                return;
            console.send_stop ();
        }
    );
    application.run ();

    connect (
        & service,
        & service_platform_test::start_,
        & service,
        [& service] ()
        {
            Q_EMIT service.failed_to_start (
                application_error
                {
                    application_error::not_service,
                    QStringLiteral ("Failed to start. Emulating not a service error.")
                }
            );
        }
    );
    connect (& application, & application::failed, & application, & application::ignore_error);

    QVERIFY (start.wait ());
    QVERIFY (application.running_as_service ().has_value ());
    QCOMPARE (application.running_as_service ().value (), false);
    QVERIFY (not application.service_configuration ().has_value ());
    QVERIFY (application.running_as_console_application ().has_value ());
    QCOMPARE (application.running_as_console_application ().value (), true);
    QVERIFY (not application.error ().has_value ());
    QVERIFY (not failed.isEmpty ());

    state_changed.wait (service_state::stopped);
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
}

void test_application::setting_no_running_as_service_runs_as_console_application ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    console_platform_test console;
    application application;
    QSignalSpy start (& application, & application::start);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    connect (& application, & application::start, & application, & application::set_started);
    connect (& application, & application::stop, & application, & application::set_stopped);
    connect (& application, & application::start, & application, & application::shut_down);
    application.set_no_running_as_service ().run ();

    QVERIFY (start.wait ());
    QVERIFY (application.running_as_service ().has_value ());
    QCOMPARE (application.running_as_service ().value (), false);
    QVERIFY (not application.service_configuration ().has_value ());
    QVERIFY (application.running_as_console_application ().has_value ());
    QCOMPARE (application.running_as_console_application ().value (), true);
    QVERIFY (not application.error ().has_value ());
    QVERIFY (failed.isEmpty ());

    state_changed.wait (service_state::stopped);
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
    QVERIFY (service.checked.isEmpty ());
    QVERIFY (service.started_.isEmpty ());
    QVERIFY (service.stopped_.isEmpty ());
}

void test_application::setting_no_retrieving_configuration_skips_retrieving_configuration ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    application application;
    QSignalSpy start (& application, & application::start);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    connect (& application, & application::start, & application, & application::set_started);
    connect (& application, & application::stop, & application, & application::set_stopped);
    connect (
        & application,
        & application::state_changed,
        & application,
        [& application, & service] ()
        {
            if (not application.state ().serving ())
                return;
            service.send_stop ();
        }
    );
    application.set_no_retrieving_service_configuration ().run ();

    state_changed.wait (service_state::stopped);
    QVERIFY (not start.isEmpty ());
    QVERIFY (application.running_as_service ().has_value ());
    QCOMPARE (application.running_as_service ().value (), true);
    QVERIFY (not application.service_configuration ().has_value ());
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
    QVERIFY (failed.isEmpty ());
    QVERIFY (service.configuration_retrieved_.isEmpty ());
}

void test_application::ignoring_failed_to_retrieve_configuration_error_runs ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    application application;
    QSignalSpy start (& application, & application::start);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    connect (& application, & application::start, & application, & application::set_started);
    connect (& application, & application::stop, & application, & application::set_stopped);
    connect (
        & application,
        & application::state_changed,
        & application,
        [& application, & service] ()
        {
            if (not application.state ().serving ())
                return;
            service.send_stop ();
        }
    );
    application.run ();

    connect (
        & service,
        & service_platform_test::retrieve_configuration_,
        & service,
        [& service] ()
        {
            Q_EMIT service.failed_to_retrieve_configuration (
                application_error
                {
                    application_error::failed_to_retrieve_configuration,
                    QStringLiteral ("Failed to retrieve service configuration. Emulating failed to retrieve configuration error.")
                }
            );
        }
    );
    connect (& application, & application::failed, & application, & application::ignore_error);

    state_changed.wait (service_state::stopped);
    QVERIFY (not start.isEmpty ());
    QVERIFY (application.running_as_service ().has_value ());
    QCOMPARE (application.running_as_service ().value (), true);
    QVERIFY (not application.service_configuration ().has_value ());
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
    QVERIFY (not failed.isEmpty ());
}

void test_application::setting_no_running_as_console_application_runs ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    console_platform_test console;
    application application;
    QSignalSpy start (& application, & application::start);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    connect (& application, & application::start, & application, & application::set_started);
    connect (& application, & application::stop, & application, & application::set_stopped);
    connect (& application, & application::start, & application, & application::shut_down);
    application.set_no_running_as_service ().set_no_running_as_console_application ().run ();

    QVERIFY (start.wait ());
    QVERIFY (application.running_as_service ().has_value ());
    QCOMPARE (application.running_as_service ().value (), false);
    QVERIFY (not application.service_configuration ().has_value ());
    QVERIFY (application.running_as_console_application ().has_value ());
    QCOMPARE (application.running_as_console_application ().value (), false);
    QVERIFY (not application.error ().has_value ());
    QVERIFY (failed.isEmpty ());

    state_changed.wait (service_state::stopped);
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
    QVERIFY (service.checked.isEmpty ());
    QVERIFY (service.started_.isEmpty ());
    QVERIFY (service.stopped_.isEmpty ());
    QVERIFY (console.started_.isEmpty ());
    QVERIFY (console.stopped_.isEmpty ());
}

void test_application::failing_to_start_platform_shuts_down_1 ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    application application;
    QSignalSpy start (& application, & application::start);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    application.run ();

    connect (
        & service,
        & service_platform_test::start_,
        & service,
        [& service] ()
        {
            Q_EMIT service.failed_to_start (
                application_error
                {
                    application_error::failed_to_run,
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

void test_application::failing_to_start_platform_shuts_down_2 ()
{
    event_loop_controller_test event_loop;
    console_platform_test console;
    application application;
    QSignalSpy start (& application, & application::start);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    application.set_no_running_as_service ().run ();

    connect (
        & console,
        & console_platform_test::start_,
        & console,
        [& console] ()
        {
            Q_EMIT console.failed_to_start (
                application_error
                {
                    application_error::failed_to_run,
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

void test_application::failing_to_set_state_serving_shuts_down ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    application application;
    QSignalSpy start (& application, & application::start);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    connect (& application, & application::start, & application, & application::set_started);
    connect (& application, & application::stop, & application, & application::set_stopped);
    application.run ();

    connect (
        & service,
        & service_platform_test::set_state_serving_,
        & service,
        [& service] ()
        {
            Q_EMIT service.failed_to_set_state_serving (
                application_error
                {
                    application_error::failed_to_run,
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

void test_application::exiting_application_shuts_down ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    application application;
    serving_state_changes state_changed (& application);

    connect (& application, & application::start, & application, & application::set_started);
    connect (& application, & application::stop, & application, & application::set_stopped);
    application.run ();

    QVERIFY (state_changed.wait (service_state::serving));
    Q_EMIT event_loop.exiting ();

    QVERIFY (state_changed.wait (service_state::stopped));
    QVERIFY (event_loop.exited.isEmpty ());
}

void test_application::shutting_down_in_initial_state_exits_1 ()
{
    event_loop_controller_test event_loop;
    application application;
    QSignalSpy start (& application, & application::start);
    QSignalSpy stop (& application, & application::stop);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    application.shut_down ();

    state_changed.wait (service_state::stopped);
    QVERIFY (start.isEmpty ());
    QVERIFY (stop.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
    QVERIFY (failed.isEmpty ());
    QCOMPARE (event_loop.exited.count (), 1);
    QCOMPARE (event_loop.exited.at (0).at (0), 0);
}

void test_application::shutting_down_in_initial_state_exits_2 ()
{
    #if not defined NDEBUG
    QSKIP ("Expected to abort.");
    #endif

    event_loop_controller_test event_loop;
    application application;
    QSignalSpy start (& application, & application::start);
    QSignalSpy stop (& application, & application::stop);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    application.shut_down ();
    application.run (); // Should cause no effect.

    state_changed.wait (service_state::stopped);
    QVERIFY (start.isEmpty ());
    QVERIFY (stop.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
    QVERIFY (failed.isEmpty ());
    QCOMPARE (event_loop.exited.count (), 1);
    QCOMPARE (event_loop.exited.at (0).at (0), 0);
}

void test_application::shutting_down_in_initial_state_exits_3 ()
{
    event_loop_controller_test event_loop;
    application application;
    QSignalSpy start (& application, & application::start);
    QSignalSpy stop (& application, & application::stop);
    serving_state_changes state_changed (& application);
    QSignalSpy failed (& application, & application::failed);

    application.run ();
    application.shut_down ();

    state_changed.wait (service_state::stopped);
    QVERIFY (start.isEmpty ());
    QVERIFY (stop.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::none_to_stopped ());
    QVERIFY (failed.isEmpty ());
    QCOMPARE (event_loop.exited.count (), 1);
    QCOMPARE (event_loop.exited.at (0).at (0), 0);
}

void test_application::reentering_event_loop_does_not_lock_lifecycle ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    application application;
    QSignalSpy start (& application, & application::start);
    QSignalSpy stop (& application, & application::stop);
    serving_state_changes state_changed (& application);

    application.run ();

    connect (
        & application,
        & application::start,
        & application,
        [& application] ()
        {
            application.set_started ();
            do
                QCoreApplication::processEvents (QEventLoop::AllEvents bitor QEventLoop::WaitForMoreEvents);
            while (not application.state ().stopped ());
        }
    );
    connect (
        & application,
        & application::stop,
        & application,
        [& application] ()
        {
            application.set_stopped ();
            do
                QCoreApplication::processEvents (QEventLoop::AllEvents bitor QEventLoop::WaitForMoreEvents);
            while (not application.state ().stopped ());
        }
    );
    connect (
        & application,
        & application::state_changed,
        & application,
        [& application, & service] ()
        {
            if (not application.state ().serving ())
                return;
            service.send_stop ();
            do
                QCoreApplication::processEvents (QEventLoop::AllEvents bitor QEventLoop::WaitForMoreEvents);
            while (not application.state ().stopped ());
        }
    );

    QVERIFY (state_changed.wait (service_state::stopped));
    QVERIFY (not start.isEmpty ());
    QVERIFY (not stop.isEmpty ());
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());
}

void test_application::receiving_event_while_proceeding_reenters ()
{
    event_loop_controller_test event_loop;
    service_platform_test service;
    application application;
    serving_state_changes state_changed (& application);

    connect (& application, & application::start, & application, & application::set_started);
    connect (& application, & application::stop, & application, & application::set_stopped);
    application.run ();

    static QtMessageHandler log (nullptr);
    static service_platform_test * service_ (nullptr);
    service_ = & service;
    // The easiest way to squeeze into 'proceed ()' to get control.
    log = qInstallMessageHandler (
        [] (const QtMsgType type, const QMessageLogContext & context, const QString & message)
        {
            log (type, context, message);

            if (message != QStringLiteral ("Serving..."))
                return;
            // 'shut_down ()' or anything that calls 'proceed_from_event_loop ()' internally must be processed.
            service_->send_stop ();
        }
    );

    QVERIFY (state_changed.wait (service_state::stopped));
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());

    qInstallMessageHandler (log);
    service_ = nullptr;
}

void test_application::system_events_logged_while_stopping ()
{
    event_loop_controller_test event_loop;
    console_platform_test console;
    application application;
    serving_state_changes state_changed (& application);

    connect (& application, & application::start, & application, & application::set_started);
    connect (& application, & application::stop, & application, & application::set_stopped);
    connect (
        & application,
        & application::state_changed,
        & application,
        [& application, & console] ()
        {
            if (not application.state ().serving ())
                return;
            console.send_stop ();
        }
    );
    application.set_no_running_as_service ().run ();

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
    QCOMPARE (application.state ().state, service_state::stopping);
    Q_EMIT console.event_received (
        application_system_event
        {
            application_system_event::stop,
            QStringLiteral ("test 2")
        }
    );

    QVERIFY (state_changed.wait (service_state::stopped));
    QCOMPARE (state_changed.changes, serving_state_changes::serving_to_stopped ());

    qInstallMessageHandler (log);
    console_ = nullptr;
}

void test_application::destroying_incorrectly_does_not_crash_1 ()
{
    #if not defined NDEBUG
    QSKIP ("Expected to abort.");
    #endif

    event_loop_controller_test event_loop;
    service_platform_test service;
    application * application (new class application);

    application->run ();

    connect (
        application,
        & application::start,
        application,
        [application] ()
        {
            delete application;
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

class event_loop_controller_plugin_test : public event_loop_controller_plugin
{
    public :
    event_loop_controller_plugin_test (QObject * parent = nullptr);

    public :
    event_loop_controller * create (QObject * parent) override;

    private :
    event_loop_controller * controller = nullptr;
    friend class event_loop_controller_test;

    private :
    Q_OBJECT
    Q_PLUGIN_METADATA (IID "background.event_loop_controller_plugin")
    Q_INTERFACES (background::event_loop_controller_plugin)
};

Q_IMPORT_PLUGIN (event_loop_controller_plugin_test)

event_loop_controller_plugin_test::event_loop_controller_plugin_test (QObject * const parent)
    : event_loop_controller_plugin (parent)
{}

event_loop_controller * event_loop_controller_plugin_test::create (QObject * const parent)
{
    Q_UNUSED (parent)
    return controller;
}

event_loop_controller_test::event_loop_controller_test ()
    : event_loop_controller (nullptr),
    exited (this, & event_loop_controller_test::exit_)
{
    plugin<event_loop_controller_plugin_test> ()->controller = this;
}

event_loop_controller_test::~event_loop_controller_test ()
{
    plugin<event_loop_controller_plugin_test> ()->controller = nullptr;
}

void event_loop_controller_test::exit (const int exit_code)
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
    Q_EMIT event_received (application_system_event { application_system_event::stop, QStringLiteral ("test") });
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
    Q_EMIT event_received (application_system_event { application_system_event::stop, QStringLiteral ("test") });
}

serving_state_changes::serving_state_changes (application * const application)
    : changed (std::nullopt),
    application_ (application)
{
    QObject::connect (
        application,
        & application::state_changed,
        application,
        [this, application] ()
        {
            changes.push_back (application->state ());
        }
    );
}

bool serving_state_changes::wait (service_state value)
{
    if (not changed.has_value ())
        changed.emplace (application_, & application::state_changed);
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

QTEST_MAIN (test_application)

#include "test_application.moc"
