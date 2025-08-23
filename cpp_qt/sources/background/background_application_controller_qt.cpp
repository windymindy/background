#include "background_application_controller_qt.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QtPlugin>

namespace background
{

application_controller_qt::application_controller_qt (QObject * const parent)
    : application_controller (parent)
{
    connect (
        QCoreApplication::instance (), & QCoreApplication::aboutToQuit,
        this, & application_controller_qt::exiting
    );
    connect (
        this, & application_controller_qt::exit_,
        QCoreApplication::instance (), & QCoreApplication::exit,
        Qt::ConnectionType::AutoConnection
    );
}

void application_controller_qt::exit (const int exit_code)
{
    Q_EMIT exit_ (exit_code);
}

background_application_controller_plugin_qt::background_application_controller_plugin_qt (QObject * const parent)
    : application_controller_plugin (parent)
{}

application_controller * background_application_controller_plugin_qt::create (QObject * const parent)
{
    return new application_controller_qt (parent);
}

} // namespace background

Q_IMPORT_PLUGIN (background_application_controller_plugin_qt)
