#pragma once
#include <QByteArray>
#include <QObject>
#include <QString>

// Abstract byte transport. The app talks to a device through this interface,
// so the simulator, a serial port (or later TCP) are interchangeable.
class Transport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~Transport() override = default;

    virtual bool open(QString *error) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual void write(const QByteArray &data) = 0;
    virtual QString description() const = 0;

signals:
    void bytesReceived(const QByteArray &data);
    void failed(const QString &message);
};
