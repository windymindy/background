#pragma once

#include <QtCore/QObject>

#include "background_library.hpp"
#include "background_datatypes_forward.hpp"

namespace background
{

class service_implementation;

class background_library service : public QObject
{
    public :
    explicit service (QObject * parent = nullptr);
    ~service ();

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
    const std::optional<service_configuration> & configuration () const;

    const std::optional<service_error> & error () const;

    int exit_code () const;
    void set_exit_code (int exit_code);

    public :
    bool with_stop_starting () const;
    service & set_with_stop_starting ();
    bool with_running_as_console_application () const;
    service & set_with_running_as_console_application ();
    bool no_running_as_service () const;
    service & set_no_running_as_service ();
    bool no_retrieving_configuration () const;
    service & set_no_retrieving_configuration ();

    private :
    Q_OBJECT
    Q_DISABLE_COPY (service)
    const QScopedPointer<service_implementation> this_;
    friend class service_implementation;
};

} // namespace background
