#pragma once

#include <QtCore/QObject>

#include "background_library.hpp"

namespace background
{

class background_library application_controller : public QObject
{
    public :
    explicit application_controller (QObject * parent);

    public Q_SLOTS :
    virtual void exit (int exit_code) = 0;

    Q_SIGNALS :
    void exiting ();

    private :
    Q_OBJECT
    Q_DISABLE_COPY (application_controller)
};

class background_library application_controller_plugin : public QObject
{
    public :
    explicit application_controller_plugin (QObject * parent = nullptr);

    public :
    virtual application_controller * create (QObject * parent = nullptr) = 0;

    private :
    Q_OBJECT
    Q_DISABLE_COPY (application_controller_plugin)
};

} // namespace background

Q_DECLARE_INTERFACE (background::application_controller_plugin, "background.application_controller_plugin")

namespace background
{

inline application_controller::application_controller (QObject * const parent)
    : QObject (parent)
{}

inline application_controller_plugin::application_controller_plugin (QObject * const parent)
    : QObject (parent)
{}

} // namespace background
