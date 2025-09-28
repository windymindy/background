#pragma once

#include <QtCore/QObject>

class logger : public QObject
{
    public :
    explicit logger (QObject * parent = nullptr);
    ~logger ();

    public :
    void set_up_logging_to_file ();
    void set_back_to_logging_to_console ();

    protected :
    void accumulate_messages_until_started ();

    Q_SIGNALS :
    void log (const QString & message);

    protected Q_SLOTS :
    void log_ (const QString & message);
    void log_while_starting (const QString & message);

    private :
    Q_OBJECT
    Q_DISABLE_COPY (logger)
};
