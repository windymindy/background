#include "background_event_loop_controller_qt.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QtPlugin>

namespace background
{

event_loop_controller_qt::event_loop_controller_qt (QObject * const parent)
    : event_loop_controller (parent)
{
    connect (
        QCoreApplication::instance (), & QCoreApplication::aboutToQuit,
        this, & event_loop_controller_qt::exiting
    );
    connect (
        this, & event_loop_controller_qt::exit_,
        QCoreApplication::instance (), & QCoreApplication::exit,
        Qt::ConnectionType::AutoConnection
    );
}

void event_loop_controller_qt::exit (const int exit_code)
{
    Q_EMIT exit_ (exit_code);
}

background_event_loop_controller_plugin_qt::background_event_loop_controller_plugin_qt (QObject * const parent)
    : event_loop_controller_plugin (parent)
{}

event_loop_controller * background_event_loop_controller_plugin_qt::create (QObject * const parent)
{
    return new event_loop_controller_qt (parent);
}

} // namespace background

Q_IMPORT_PLUGIN (background_event_loop_controller_plugin_qt)
