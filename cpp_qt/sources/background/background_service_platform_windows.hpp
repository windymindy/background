#pragma once

#if not defined QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include "background_service_platform.hpp"

namespace background
{

class service_platform_windows : public service_platform
{
    public :
    explicit service_platform_windows (QObject * parent);
    ~service_platform_windows ();

    public Q_SLOTS :
    bool check () override;

    void start () override;
    void stop () override;

    void set_state_serving () override;
    void set_state_stopping () override;
    void set_state_stopped (int exit_code) override;

    void retrieve_configuration () override;

    Q_SIGNALS :
    void proceed_ ();
    void event_received_ (unsigned long event);

    protected Q_SLOTS :
    void process_event (unsigned long event);

    private :
    Q_OBJECT
    Q_DISABLE_COPY (service_platform_windows)
};

class background_service_platform_plugin_windows : public service_platform_plugin
{
    public :
    explicit background_service_platform_plugin_windows (QObject * parent = nullptr);

    public :
    unsigned int order () const override;
    bool detect () override;
    service_platform * create (QObject * parent = nullptr) override;

    private :
    Q_OBJECT
    Q_DISABLE_COPY (background_service_platform_plugin_windows)
    Q_PLUGIN_METADATA (IID "background.service_platform_plugin")
    Q_INTERFACES (background::service_platform_plugin)
};

} // namespace background
