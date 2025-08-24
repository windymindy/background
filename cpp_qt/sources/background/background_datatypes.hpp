#pragma once

#include <QtCore/QString>

namespace background
{

struct service_configuration
{
    QString name;
    QString description;
    QString executable;
    QString user;
};

enum struct service_state : unsigned int
{
    none,
    starting,
    serving,
    stopping,
    stopped
};

enum struct target_service_state : unsigned int
{
    none,
    serving,
    stopped
};

struct serving_state
{
    service_state state;
    target_service_state target_state;

    bool none () const;
    bool serving () const;
    bool stopped () const;

    // c++20 default comparisons
    //friend bool operator == (serving_state, serving_state) = default;
};

struct application_error
{
    enum error
    {
        not_service,
        failed_to_retrieve_configuration,
        failed_to_run
    };

    error error;
    QString text;

    bool recoverable () const;
};

struct application_system_event
{
    // Maybe extended with reload_configuration, pause or anything.
    enum action
    {
        stop
    };

    action action;
    QString name;
};

} // namespace background

namespace background
{

inline bool serving_state::none () const
{
    return state == service_state::none and target_state == target_service_state::none;
}

inline bool serving_state::serving () const
{
    return state == service_state::serving and target_state == target_service_state::none;
}

inline bool serving_state::stopped () const
{
    return state == service_state::stopped and target_state == target_service_state::none;
}

inline bool operator == (const serving_state value_1, const serving_state value_2)
{
    return value_1.state == value_2.state and value_1.target_state == value_2.target_state;
}

inline bool operator != (const serving_state value_1, const serving_state value_2)
{
    return value_1.state != value_2.state or value_1.target_state != value_2.target_state;
}

inline bool application_error::recoverable () const
{
    switch (error)
    {
        case not_service :
        case failed_to_retrieve_configuration : return true;
        case failed_to_run :
        default : return false;
    }
}

} // namespace background
