#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtCore/QDebug>

#include <background/application>
//#include <background/service_controller>
//#include <background/system_logger>

#include "logger.hpp"

int main (int argc, char * argv [])
{
    logger logger;
    QCoreApplication application_ (argc, argv);

    // todo add installing, uninstalling and checking the service
    //  parse command line first and only then run

    background::application application;
    QTimer timer_1;
    timer_1.setSingleShot (true);
    timer_1.setInterval (std::chrono::seconds (2));
    QTimer timer_2;
    timer_2.setSingleShot (true);
    timer_2.setInterval (std::chrono::minutes (1));
    QObject::connect (
        & application, & background::application::start, & application,
        [& application, & logger, & timer_1] ()
        {
            if (application.running_as_service ().value ())
            {
                logger.set_up_logging_to_file ();

                // 'no_retrieving_configuration' is false and there is no 'ignore_error ()' call,
                // so the configuration value is guaranteed.
                const auto & configuration (application.service_configuration ().value ());
                qInfo ().noquote () << QString (
                    "Running as a service:\n"
                    "    name: '%1',\n"
                    "    description: '%2',\n"
                    "    executable: '%3',\n"
                    "    user: '%4'."
                ).arg (configuration.name, configuration.description, configuration.executable, configuration.user);
            }
            else // Alternatively, consider running not as a service an error. Print usage on 'failed ()' and quit.
            {
                logger.set_back_to_logging_to_console ();

                qInfo ("Running as a regular program.");
            }

            qInfo ("Time to spin up the example_service useful functionality. This will take some time...");
            QObject::connect (
                & timer_1, & QTimer::timeout,
                & application,
                [& application] ()
                {
                    // Simulate a failure.
                    if (QDateTime::currentDateTime ().time ().second () % 11 == 0)
                    {
                        qWarning ("Something went wrong.");
                        // todo system_logger::log ({ 111, error, "Example failure." });
                        application.set_exit_code (111);
                        application.set_failed_to_start ();
                        return;
                    }
                    qInfo ("example_service has finished initializing.");
                    // The system will be notified shortly that the service is up and running.
                    application.set_started ();
                }
            );
            timer_1.start ();
        }
    );
    QObject::connect (
        & application, & background::application::stop, & application,
        [& application, & timer_1] ()
        {
            //timer_2.stop ();
            timer_1.disconnect ();
            if (timer_1.isActive ())
            {
                timer_1.stop ();
                qInfo ("Time to stop, though example_service has not initialized yet. This will take some time...");
            }
            else
                qInfo ("Time to stop. This will take some time...");
            QObject::connect (
                & timer_1, & QTimer::timeout,
                & application,
                [& application] ()
                {
                    qInfo ("example_service has finished stopping.");
                    application.set_stopped ();
                }
            );
            timer_1.start ();
        }
    );
    QObject::connect (
        & application, & background::application::state_changed, & application,
        [& application, & timer_2] ()
        {
            if (application.state ().serving ())
            {
                qInfo ("example_service is up and running.");
                QObject::connect (
                    & timer_2, & QTimer::timeout,
                    & application,
                    [& application] ()
                    {
                        qInfo ("All the workload has been processed. Let the application down now.");
                        application.shut_down ();
                    }
                );
                timer_2.start ();
            }
            else if (application.state ().stopped ())
                qInfo ("example_service has shut down completely.");
        }
    );
    QObject::connect (
        & application, & background::application::failed, & application,
        [& application] ()
        {
            const auto & error (application.error ().value ());
            //qWarning ().noquote () << error.text;
            //if (error.recoverable ()) application.ignore_error ();
        }
    );
    application
    .set_with_stop_starting ()
    .set_with_running_as_non_service ()
    .run ();

    return application_.exec ();
}
