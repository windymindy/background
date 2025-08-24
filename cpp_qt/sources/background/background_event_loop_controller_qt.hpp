#pragma once

#if not defined QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include "background_event_loop_controller.hpp"

namespace background
{

class event_loop_controller_qt : public event_loop_controller
{
    public :
    explicit event_loop_controller_qt (QObject * parent);

    public Q_SLOTS :
    void exit (int exit_code) override;

    Q_SIGNALS :
    void exit_ (int exit_code);

    private :
    Q_OBJECT
    Q_DISABLE_COPY (event_loop_controller_qt)
};

class background_event_loop_controller_plugin_qt : public event_loop_controller_plugin
{
    public :
    background_event_loop_controller_plugin_qt (QObject * parent = nullptr);

    public :
    event_loop_controller * create (QObject * parent) override;

    private :
    Q_OBJECT
    Q_DISABLE_COPY (background_event_loop_controller_plugin_qt)
    Q_PLUGIN_METADATA (IID "background.event_loop_controller_plugin")
    Q_INTERFACES (background::event_loop_controller_plugin)
};

} // namespace background
