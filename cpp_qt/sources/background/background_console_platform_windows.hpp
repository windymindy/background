#pragma once

#if not defined QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include "background_console_platform.hpp"

namespace background
{

class console_platform_windows : public console_platform
{
    public :
    explicit console_platform_windows (QObject * parent);
    ~console_platform_windows ();

    public Q_SLOTS :
    void start () override;
    void stop () override;

    Q_SIGNALS :
    void event_received_ (unsigned long event);

    protected Q_SLOTS :
    void process_event (unsigned long event);

    private :
    Q_OBJECT
    Q_DISABLE_COPY (console_platform_windows)
};

class background_console_platform_plugin_windows : public console_platform_plugin
{
    public :
    explicit background_console_platform_plugin_windows (QObject * parent = nullptr);

    public :
    unsigned int order () const override;
    console_platform * create (QObject * parent = nullptr) override;

    private :
    Q_OBJECT
    Q_DISABLE_COPY (background_console_platform_plugin_windows)
    Q_PLUGIN_METADATA (IID "background.console_platform_plugin")
    Q_INTERFACES (background::console_platform_plugin)
};

} // namespace background
