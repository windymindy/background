#pragma once

#include <QtCore/QObject>

#include "background_library.hpp"
#include "background_datatypes_forward.hpp"

namespace background
{

class background_library service_platform : public QObject
{
    public :
    explicit service_platform (QObject * parent);

    public Q_SLOTS :
    virtual bool check () = 0;

    virtual void start () = 0;
    virtual void stop () = 0;

    virtual void set_state_serving () = 0;
    virtual void set_state_stopping () = 0;
    virtual void set_state_stopped (int exit_code) = 0;

    virtual void retrieve_configuration () = 0;

    Q_SIGNALS :
    void started ();
    void failed_to_start (const application_error & error); // clazy:exclude=fully-qualified-moc-types

    void stopped ();

    void state_serving_set ();
    void failed_to_set_state_serving (const application_error & error); // clazy:exclude=fully-qualified-moc-types
    void state_stopping_set ();
    void state_stopped_set ();

    void configuration_retrieved (const service_configuration & configuration); // clazy:exclude=fully-qualified-moc-types
    void failed_to_retrieve_configuration (const application_error & error); // clazy:exclude=fully-qualified-moc-types

    void event_received (const application_system_event & event); // clazy:exclude=fully-qualified-moc-types

    private :
    Q_OBJECT
    Q_DISABLE_COPY (service_platform)
};

class background_library service_platform_plugin : public QObject
{
    public :
    explicit service_platform_plugin (QObject * parent = nullptr);

    public :
    virtual unsigned int order () const = 0;
    virtual bool detect () = 0;
    virtual service_platform * create (QObject * parent = nullptr) = 0;

    private :
    Q_OBJECT
    Q_DISABLE_COPY (service_platform_plugin)
};

} // namespace background

Q_DECLARE_INTERFACE (background::service_platform_plugin, "background.service_platform_plugin")

namespace background
{

inline service_platform::service_platform (QObject * const parent)
    : QObject (parent)
{}

inline service_platform_plugin::service_platform_plugin (QObject * const parent)
    : QObject (parent)
{}

} // namespace background
