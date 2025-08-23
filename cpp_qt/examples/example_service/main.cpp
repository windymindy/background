#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>

#include <background/service>
//#include <background/service_controller>
//#include <background/system_logger>

void set_up_service_logging ();

int main (int argc, char * argv [])
{
    QCoreApplication application (argc, argv);

    // todo add installing, uninstalling and checking the service
    //  parse command line first and only then run

    background::service service;
    QTimer timer_1;
    timer_1.setSingleShot (true);
    timer_1.setInterval (std::chrono::seconds (2));
    QTimer timer_2;
    timer_2.setSingleShot (true);
    timer_2.setInterval (std::chrono::minutes (1));
    QObject::connect (
        & service, & background::service::start, & service,
        [& service, & timer_1] ()
        {
            if (service.running_as_service ().value ())
            {
                set_up_service_logging ();

                // 'no_retrieving_configuration' is false and there is no 'ignore_error ()' call,
                // so the configuration value is guaranteed.
                const auto & configuration (service.configuration ().value ());
                qInfo ().noquote () << QString (
                    "Running as a system service:\n"
                    "    name: '%1',\n"
                    "    description: '%2',\n"
                    "    executable: '%3',\n"
                    "    user: '%4'."
                ).arg (configuration.name, configuration.description, configuration.executable, configuration.user);
            }
            else // Alternatively, consider running not as a system service an error. Print usage on 'failed ()' and quit.
                qInfo ("Running as a regular program.");

            qInfo ("Time to spin up the example_service useful functionality. This will take some time...");
            QObject::connect (
                & timer_1, & QTimer::timeout,
                & service,
                [& service] ()
                {
                    // Simulate a failure.
                    if (QDateTime::currentDateTime ().time ().second () % 11 == 0)
                    {
                        qWarning ("Something went wrong.");
                        // todo system_logger::log ({ 111, error, "Example failure." });
                        service.set_exit_code (111);
                        service.set_failed_to_start ();
                        return;
                    }
                    qInfo ("example_service has finished initializing.");
                    // The system will be notified shortly that the service is up and running.
                    service.set_started ();
                }
            );
            timer_1.start ();
        }
    );
    QObject::connect (
        & service, & background::service::stop, & service,
        [& service, & timer_1] ()
        {
            //timer_2.stop ();
            timer_1.disconnect ();
            if (timer_1.isActive ())
            {
                timer_1.stop ();
                qInfo ("Time to stop, though the example_service has not initialized yet. This will take some time...");
            }
            else
                qInfo ("Time to stop. This will take some time...");
            QObject::connect (
                & timer_1, & QTimer::timeout,
                & service,
                [& service] ()
                {
                    qInfo ("example_service has finished stopping.");
                    service.set_stopped ();
                }
            );
            timer_1.start ();
        }
    );
    QObject::connect (
        & service, & background::service::state_changed, & service,
        [& service, & timer_2] ()
        {
            if (service.state ().serving ())
            {
                qInfo ("example_service is up and running.");
                QObject::connect (
                    & timer_2, & QTimer::timeout,
                    & service,
                    [& service] ()
                    {
                        qInfo ("All the workload has been processed. Let the service down now.");
                        service.shut_down ();
                    }
                );
                timer_2.start ();
            }
            else if (service.state ().stopped ())
                qInfo ("example_service has shut down completely.");
        }
    );
    QObject::connect (
        & service, & background::service::failed, & service,
        [& service] ()
        {
            const auto & error (service.error ().value ());
            // todo qWarning ().noquote () << error.text;
            //if (error.recoverable ()) service.ignore_error ();
        }
    );
    service
    .set_with_stop_starting ()
    .set_with_running_as_console_application ()
    .run ();

    return application.exec ();
}

void log (const QtMsgType type, const QMessageLogContext & context, const QString & message_)
{
    // todo
}

void set_up_service_logging ()
{
    // todo
    qInstallMessageHandler (& log);
}

// Providing blocking start and stop callbacks.
class my_service : public background::service
{
public :
    my_service ()
        : service ()
    {
        connect (
            this, & background::service::start, this,
            [this] ()
            {
                if (Q_EMIT start_blocking ())
                    set_started ();
                else
                    set_failed_to_start ();
            }
        );
        connect (
            this, & background::service::stop, this,
            [this] ()
            {
                Q_EMIT stop_blocking ();
                set_stopped ();
            }
        );
    }

signals :
    bool start_blocking ();
    void stop_blocking ();

private :
    using background::service::set_with_stop_starting;

private :
    //Q_OBJECT
    Q_DISABLE_COPY (my_service)
};
