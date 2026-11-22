#pragma once
#include "Transport.h"
#include "core/protocol.h"
#include <QObject>

// Speaks the framed protocol over any Transport: sends Get/Set requests,
// parses replies and telemetry, and reports them as Qt signals.
class DeviceClient : public QObject {
    Q_OBJECT
public:
    explicit DeviceClient(QObject *parent = nullptr) : QObject(parent) {}

    void setTransport(Transport *t);   // pass nullptr to detach
    void requestGet(quint8 id);
    void requestSet(quint8 id, qint32 value);

signals:
    void valueReceived(quint8 id, qint32 value);
    void deviceError(quint8 id, quint8 code);
    void telemetryReceived(quint32 seq, qint32 reading);
    void crcErrorsChanged(int total);

private:
    void onBytes(const QByteArray &data);
    void handle(const dev::Frame &f);
    void send(const dev::Frame &f);

    Transport *transport_ = nullptr;
    dev::FrameParser parser_;
    size_t lastCrcErrors_ = 0;
};
