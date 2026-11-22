#include "DeviceClient.h"

void DeviceClient::setTransport(Transport *t) {
    if (transport_) disconnect(transport_, nullptr, this, nullptr);
    transport_ = t;
    parser_ = dev::FrameParser();
    lastCrcErrors_ = 0;
    if (transport_)
        connect(transport_, &Transport::bytesReceived, this, &DeviceClient::onBytes);
}

void DeviceClient::send(const dev::Frame &f) {
    if (!transport_) return;
    const auto bytes = dev::encode(f);
    transport_->write(QByteArray(reinterpret_cast<const char *>(bytes.data()),
                                 static_cast<qsizetype>(bytes.size())));
}

void DeviceClient::requestGet(quint8 id) {
    dev::Frame f;
    f.cmd = dev::Get;
    f.payload = {id};
    send(f);
}

void DeviceClient::requestSet(quint8 id, qint32 value) {
    dev::Frame f;
    f.cmd = dev::Set;
    f.payload = {id};
    dev::putI32(f.payload, value);
    send(f);
}

void DeviceClient::onBytes(const QByteArray &data) {
    parser_.feed(reinterpret_cast<const uint8_t *>(data.constData()), static_cast<size_t>(data.size()));
    dev::Frame f;
    while (parser_.next(f)) handle(f);
    if (parser_.crcErrors() != lastCrcErrors_) {
        lastCrcErrors_ = parser_.crcErrors();
        emit crcErrorsChanged(static_cast<int>(lastCrcErrors_));
    }
}

void DeviceClient::handle(const dev::Frame &f) {
    switch (f.cmd) {
    case dev::Value:
        if (f.payload.size() == 5) emit valueReceived(f.payload[0], dev::getI32(&f.payload[1]));
        break;
    case dev::Error:
        if (f.payload.size() == 2) emit deviceError(f.payload[0], f.payload[1]);
        break;
    case dev::Telemetry:
        if (f.payload.size() == 8)
            emit telemetryReceived(dev::getU32(&f.payload[0]), dev::getI32(&f.payload[4]));
        break;
    default:
        break;
    }
}
