#include "SimTransport.h"
#include <QMetaObject>

SimTransport::SimTransport(QObject *parent) : Transport(parent) {
    telemetryTimer_.setInterval(100);  // 10 Hz telemetry
    connect(&telemetryTimer_, &QTimer::timeout, this, &SimTransport::sendTelemetry);
}

bool SimTransport::open(QString *) {
    sim_.reset();
    clock_.start();
    telemetryTimer_.start();
    open_ = true;
    return true;
}

void SimTransport::close() {
    telemetryTimer_.stop();
    open_ = false;
}

void SimTransport::write(const QByteArray &data) {
    if (!open_) return;
    const auto reply = sim_.receive(reinterpret_cast<const uint8_t *>(data.constData()),
                                    static_cast<size_t>(data.size()));
    if (reply.empty()) return;
    const QByteArray out(reinterpret_cast<const char *>(reply.data()),
                         static_cast<qsizetype>(reply.size()));
    // Deliver asynchronously, like a real device would.
    QMetaObject::invokeMethod(this, [this, out] { emit bytesReceived(out); }, Qt::QueuedConnection);
}

void SimTransport::sendTelemetry() {
    const auto frame = sim_.makeTelemetry(static_cast<double>(clock_.elapsed()) / 1000.0);
    emit bytesReceived(QByteArray(reinterpret_cast<const char *>(frame.data()),
                                  static_cast<qsizetype>(frame.size())));
}
