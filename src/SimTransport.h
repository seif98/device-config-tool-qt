#pragma once
#include "Transport.h"
#include "core/device_sim.h"
#include <QElapsedTimer>
#include <QTimer>

// Transport backed by the in-process DeviceSimulator. No hardware needed.
class SimTransport : public Transport {
    Q_OBJECT
public:
    explicit SimTransport(QObject *parent = nullptr);

    bool open(QString *error) override;
    void close() override;
    bool isOpen() const override { return open_; }
    void write(const QByteArray &data) override;
    QString description() const override { return QStringLiteral("Simulated device"); }

private:
    void sendTelemetry();

    dev::DeviceSimulator sim_;
    QTimer telemetryTimer_;
    QElapsedTimer clock_;
    bool open_ = false;
};
