#include "SerialTransport.h"

SerialTransport::SerialTransport(const QString &portName, qint32 baudRate, QObject *parent)
    : Transport(parent) {
    port_.setPortName(portName);
    port_.setBaudRate(baudRate);
    port_.setDataBits(QSerialPort::Data8);
    port_.setParity(QSerialPort::NoParity);
    port_.setStopBits(QSerialPort::OneStop);
    port_.setFlowControl(QSerialPort::NoFlowControl);

    connect(&port_, &QSerialPort::readyRead, this, [this] { emit bytesReceived(port_.readAll()); });
    connect(&port_, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError e) {
        if (e != QSerialPort::NoError) emit failed(port_.errorString());
    });
}

bool SerialTransport::open(QString *error) {
    if (port_.open(QIODevice::ReadWrite)) return true;
    if (error) *error = port_.errorString();
    return false;
}

void SerialTransport::close() {
    if (port_.isOpen()) port_.close();
}

void SerialTransport::write(const QByteArray &data) {
    if (port_.isOpen()) port_.write(data);
}

QString SerialTransport::description() const {
    return QStringLiteral("%1 @ %2").arg(port_.portName()).arg(port_.baudRate());
}
