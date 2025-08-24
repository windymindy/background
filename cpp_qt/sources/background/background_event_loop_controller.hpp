#pragma once

#include <QtCore/QObject>

#include "background_library.hpp"

namespace background
{

class background_library event_loop_controller : public QObject
{
    public :
    explicit event_loop_controller (QObject * parent);

    public Q_SLOTS :
    virtual void exit (int exit_code) = 0;

    Q_SIGNALS :
    void exiting ();

    private :
    Q_OBJECT
    Q_DISABLE_COPY (event_loop_controller)
};

class background_library event_loop_controller_plugin : public QObject
{
    public :
    explicit event_loop_controller_plugin (QObject * parent = nullptr);

    public :
    virtual event_loop_controller * create (QObject * parent = nullptr) = 0;

    private :
    Q_OBJECT
    Q_DISABLE_COPY (event_loop_controller_plugin)
};

} // namespace background

Q_DECLARE_INTERFACE (background::event_loop_controller_plugin, "background.event_loop_controller_plugin")

namespace background
{

inline event_loop_controller::event_loop_controller (QObject * const parent)
    : QObject (parent)
{}

inline event_loop_controller_plugin::event_loop_controller_plugin (QObject * const parent)
    : QObject (parent)
{}

} // namespace background
