#pragma once

#include <background/application>

// Providing blocking start and stop callbacks.

class application : public background::application
{
public :
    application ()
        : background::application ()
    {
        connect (
            this, & background::application::start, this,
            [this] ()
            {
                if (Q_EMIT start_blocking ())
                    set_started ();
                else
                    set_failed_to_start ();
            }
        );
        connect (
            this, & background::application::stop, this,
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
    using background::application::set_with_stop_starting;

private :
    Q_OBJECT
    Q_DISABLE_COPY (application)
};
