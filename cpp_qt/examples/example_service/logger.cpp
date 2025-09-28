#include "logger.hpp"

#include <deque>

#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>

// Should not Qt provide logging to a file?

namespace
{

logger * instance (nullptr);
QBasicMutex mutex;
QtMessageHandler print_ (nullptr);
std::deque<QString> * messages_before_started (nullptr);
QFile * file_ (nullptr);

void log_ (const QtMsgType type, const QMessageLogContext & context, const QString & message);

} // namespace

logger::logger (QObject * const parent)
    : QObject (parent)
{
    qSetMessagePattern (QStringLiteral ("%{time} %{type} %{category} %{threadid} %{function}:%{line}\n%{message}"));
    accumulate_messages_until_started ();
}

logger::~logger ()
{
    if (instance == nullptr)
        return;
    file_ = nullptr;
    set_back_to_logging_to_console ();
}

void logger::set_up_logging_to_file ()
{
    const auto basename (QFileInfo (QCoreApplication::applicationFilePath ()).baseName ());
    const QDir directory (QCoreApplication::applicationDirPath ());
    const auto current_ (directory.absoluteFilePath (basename).append (QStringLiteral (".log.txt")));
    {
        QFile current (current_);
        if (current.exists ())
        {
            const auto previous_ (directory.absoluteFilePath (basename).append (QStringLiteral (".log.previous.txt")));
            QFile previous (previous_);
            if (previous.exists ())
                previous.remove ();
            current.rename (previous_);
        }
    }
    file_ = new QFile (current_, this);
    if (not file_->open (QIODevice::WriteOnly bitor QIODevice::Append))
    {
        qWarning (
            "Failed to open log file '%s': %s",
            qUtf8Printable (file_->fileName ()),
            qUtf8Printable (file_->errorString ())
        );
        delete file_;
        file_ = nullptr;
        set_back_to_logging_to_console ();
        return;
    }
    auto * const timer (new QTimer (file_));
    timer->setSingleShot (false);
    timer->setInterval (std::chrono::seconds (2));
    connect (timer, & QTimer::timeout, file_, & QFile::flush);
    timer->start ();

    disconnect (this, & logger::log, this, & logger::log_while_starting);
    connect (this, & logger::log, this, & logger::log_, Qt::QueuedConnection);

    for (const auto & message : std::as_const (* messages_before_started))
        log_ (message);
    file_->flush ();
    delete messages_before_started;
    messages_before_started = nullptr;
}

void logger::set_back_to_logging_to_console ()
{
    qInstallMessageHandler (nullptr);
    if (messages_before_started != nullptr)
    {
        delete messages_before_started;
        messages_before_started = nullptr;
    }
    QMutexLocker locker (& mutex);
    instance = nullptr;
}

void logger::accumulate_messages_until_started ()
{
    connect (this, & logger::log, this, & logger::log_while_starting, Qt::QueuedConnection);
    instance = this;
    messages_before_started = new std::deque<QString>;
    print_ = qInstallMessageHandler (& ::log_);
}

void logger::log_ (const QString & message)
{
    file_->write (message.toUtf8 ());
    file_->write (u8"\n");
}

void logger::log_while_starting (const QString & message)
{
    if (file_ != nullptr)
    {
        log_ (message);
        return;
    }
    else if (messages_before_started == nullptr)
        return;
    messages_before_started->push_back (message);
}

namespace
{

void log_ (const QtMsgType type, const QMessageLogContext & context, const QString & message)
{
    print_ (type, context, message);

    QMutexLocker locker (& mutex);
    if (instance == nullptr)
        return;
    const auto message_ (qFormatLogMessage (type, context, message));
    if (message_.isNull ())
        return;
    emit instance->log (message_);
}

} // namespace
