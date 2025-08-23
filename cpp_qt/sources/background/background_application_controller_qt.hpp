#pragma once

#if not defined QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include "background_application_controller.hpp"

namespace background
{

class application_controller_qt : public application_controller
{
    public :
    explicit application_controller_qt (QObject * parent);

    public Q_SLOTS :
    void exit (int exit_code) override;

    Q_SIGNALS :
    void exit_ (int exit_code);

    private :
    Q_OBJECT
    Q_DISABLE_COPY (application_controller_qt)
};

class background_application_controller_plugin_qt : public application_controller_plugin
{
    public :
    background_application_controller_plugin_qt (QObject * parent = nullptr);

    public :
    application_controller * create (QObject * parent) override;

    private :
    Q_OBJECT
    Q_DISABLE_COPY (background_application_controller_plugin_qt)
    Q_PLUGIN_METADATA (IID "background.application_controller_plugin")
    Q_INTERFACES (background::application_controller_plugin)
};

} // namespace background
