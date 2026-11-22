#pragma once
#include "Transport.h"
#include <QSerialPort>

// Transport over a real serial port (8N1).
class SerialTransport : public Transport {
    Q_OBJECT
public:
    SerialTransport(const QString &portName, qint32 baudRate, QObject *parent = nullptr);

    bool open(QString *error) override;
    void close() override;
    bool isOpen() const override { return port_.isOpen(); }
    void write(const QByteArray &data) override;
    QString description() const override;

private:
    QSerialPort port_;
};
