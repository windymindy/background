#pragma once

#include <QtCore/QObject>

#include "background_library.hpp"
#include "background_datatypes_forward.hpp"

namespace background
{

class application_implementation;

class background_library application : public QObject
{
    public :
    explicit application (QObject * parent = nullptr);
    ~application ();

    public Q_SLOTS :
    void run ();
    void shut_down ();

    Q_SIGNALS :
    void start ();
    void stop ();

    void state_changed ();

    void failed ();

    public Q_SLOTS :
    void set_started ();
    void set_failed_to_start ();
    void set_stopped ();

    void ignore_error ();

    public :
    serving_state state () const;

    std::optional<bool> running_as_service () const;
    const std::optional<service_configuration> & service_configuration () const;

    std::optional<bool> running_as_console_application () const;

    const std::optional<application_error> & error () const;

    int exit_code () const;
    void set_exit_code (int exit_code);

    public :
    bool with_stop_starting () const;
    application & set_with_stop_starting ();
    bool with_running_as_non_service () const;
    application & set_with_running_as_non_service ();
    bool no_retrieving_service_configuration () const;
    application & set_no_retrieving_service_configuration ();
    bool no_running_as_service () const;
    application & set_no_running_as_service ();
    bool no_running_as_console_application () const;
    application & set_no_running_as_console_application ();

    private :
    Q_OBJECT
    Q_DISABLE_COPY (application)
    const QScopedPointer<application_implementation> this_;
    friend class application_implementation;
};

} // namespace background
