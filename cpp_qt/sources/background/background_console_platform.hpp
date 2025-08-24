#pragma once

#include <QtCore/QObject>

#include "background_library.hpp"
#include "background_datatypes_forward.hpp"

namespace background
{

class background_library console_platform : public QObject
{
    public :
    explicit console_platform (QObject * parent);

    public Q_SLOTS :
    virtual void start () = 0;
    virtual void stop () = 0;

    Q_SIGNALS :
    void started ();
    void failed_to_start (const application_error & error); // clazy:exclude=fully-qualified-moc-types

    void stopped ();

    void event_received (const application_system_event & event); // clazy:exclude=fully-qualified-moc-types

    private :
    Q_OBJECT
    Q_DISABLE_COPY (console_platform)
};

class background_library console_platform_plugin : public QObject
{
    public :
    explicit console_platform_plugin (QObject * parent = nullptr);

    public :
    virtual unsigned int order () const = 0;
    virtual console_platform * create (QObject * parent = nullptr) = 0;

    private :
    Q_OBJECT
    Q_DISABLE_COPY (console_platform_plugin)
};

} // namespace background

Q_DECLARE_INTERFACE (background::console_platform_plugin, "background.console_platform_plugin")

namespace background
{

inline console_platform::console_platform (QObject * const parent)
    : QObject (parent)
{}

inline console_platform_plugin::console_platform_plugin (QObject * const parent)
    : QObject (parent)
{}

} // namespace background
