#include "MainWindow.h"
#include "SerialTransport.h"
#include "SimTransport.h"
#include "core/params.h"

#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QTableWidget>
#include <QTime>
#include <QVBoxLayout>

namespace {
enum Col { ColName, ColValue, ColMin, ColMax, ColUnit, ColStatus, ColCount };
const QColor kBad(255, 214, 214);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Device Config Tool");
    resize(900, 650);
    buildUi();
    setConnected(false);

    connect(&client_, &DeviceClient::valueReceived, this, &MainWindow::onValue);
    connect(&client_, &DeviceClient::deviceError, this, &MainWindow::onDeviceError);
    connect(&client_, &DeviceClient::telemetryReceived, this, &MainWindow::onTelemetry);
    connect(&client_, &DeviceClient::crcErrorsChanged, this, [this](int n) {
        crcErrors_ = n;
        log(QStringLiteral("CRC errors so far: %1").arg(n));
    });
}

MainWindow::~MainWindow() {
    client_.setTransport(nullptr);
    if (transport_) transport_->close();
}

void MainWindow::buildUi() {
    auto *root = new QWidget;
    setCentralWidget(root);
    auto *layout = new QVBoxLayout(root);

    // --- connection row
    auto *conn = new QHBoxLayout;
    sourceCombo_ = new QComboBox;
    sourceCombo_->addItems({"Simulator", "Serial port"});
    portCombo_ = new QComboBox;
    baudCombo_ = new QComboBox;
    for (int b : {9600, 19200, 38400, 57600, 115200, 230400}) baudCombo_->addItem(QString::number(b), b);
    baudCombo_->setCurrentText("115200");
    auto *refresh = new QPushButton("Refresh ports");
    connectBtn_ = new QPushButton("Connect");
    conn->addWidget(new QLabel("Source:"));
    conn->addWidget(sourceCombo_);
    conn->addWidget(new QLabel("Port:"));
    conn->addWidget(portCombo_, 1);
    conn->addWidget(refresh);
    conn->addWidget(new QLabel("Baud:"));
    conn->addWidget(baudCombo_);
    conn->addWidget(connectBtn_);
    layout->addLayout(conn);
    connect(connectBtn_, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(sourceCombo_, &QComboBox::currentIndexChanged, this, [this](int i) {
        portCombo_->setEnabled(i == 1);
        baudCombo_->setEnabled(i == 1);
    });
    refreshPorts();
    portCombo_->setEnabled(false);
    baudCombo_->setEnabled(false);

    // --- parameter table
    table_ = new QTableWidget(static_cast<int>(dev::kParamCount), ColCount);
    table_->setHorizontalHeaderLabels({"Parameter", "Value", "Min", "Max", "Unit", "Status"});
    table_->verticalHeader()->hide();
    table_->horizontalHeader()->setSectionResizeMode(ColStatus, QHeaderView::Stretch);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    for (int r = 0; r < static_cast<int>(dev::kParamCount); ++r) {
        const auto &p = dev::kParams[r];
        auto readOnly = [](const QString &text) {
            auto *it = new QTableWidgetItem(text);
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            return it;
        };
        table_->setItem(r, ColName, readOnly(p.name));
        table_->setItem(r, ColValue, new QTableWidgetItem(QString::number(p.def)));
        table_->setItem(r, ColMin, readOnly(QString::number(p.min)));
        table_->setItem(r, ColMax, readOnly(QString::number(p.max)));
        table_->setItem(r, ColUnit, readOnly(p.unit));
        table_->setItem(r, ColStatus, readOnly(""));
    }
    connect(table_, &QTableWidget::cellChanged, this, &MainWindow::onCellChanged);
    layout->addWidget(table_, 1);

    // --- action buttons
    auto *actions = new QHBoxLayout;
    readBtn_ = new QPushButton("Read all");
    writeBtn_ = new QPushButton("Write all");
    auto *resetBtn = new QPushButton("Reset to defaults");
    auto *importBtn = new QPushButton("Import JSON...");
    auto *exportBtn = new QPushButton("Export JSON...");
    for (auto *b : {readBtn_, writeBtn_, resetBtn, importBtn, exportBtn}) actions->addWidget(b);
    actions->addStretch(1);
    layout->addLayout(actions);
    connect(readBtn_, &QPushButton::clicked, this, &MainWindow::readAll);
    connect(writeBtn_, &QPushButton::clicked, this, &MainWindow::writeAll);
    connect(resetBtn, &QPushButton::clicked, this, &MainWindow::resetDefaults);
    connect(importBtn, &QPushButton::clicked, this, &MainWindow::importJson);
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::exportJson);

    // --- telemetry + log
    telemetryLabel_ = new QLabel("Telemetry: -");
    layout->addWidget(telemetryLabel_);
    logView_ = new QPlainTextEdit;
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(500);
    layout->addWidget(logView_, 1);
}

void MainWindow::refreshPorts() {
    portCombo_->clear();
    for (const auto &info : QSerialPortInfo::availablePorts())
        portCombo_->addItem(info.portName());
}

void MainWindow::setConnected(bool connected) {
    connectBtn_->setText(connected ? "Disconnect" : "Connect");
    readBtn_->setEnabled(connected);
    writeBtn_->setEnabled(connected);
    sourceCombo_->setEnabled(!connected);
}

void MainWindow::toggleConnection() {
    if (transport_) {                       // disconnect
        client_.setTransport(nullptr);
        transport_->close();
        transport_->deleteLater();
        transport_ = nullptr;
        setConnected(false);
        log("Disconnected");
        return;
    }
    Transport *t = nullptr;
    if (sourceCombo_->currentIndex() == 0) {
        t = new SimTransport(this);
    } else {
        if (portCombo_->currentText().isEmpty()) {
            QMessageBox::warning(this, "Serial port", "No serial port selected.");
            return;
        }
        t = new SerialTransport(portCombo_->currentText(), baudCombo_->currentData().toInt(), this);
    }
    QString err;
    if (!t->open(&err)) {
        QMessageBox::warning(this, "Connection failed", err);
        delete t;
        return;
    }
    connect(t, &Transport::failed, this, [this](const QString &m) { log("Transport error: " + m); });
    transport_ = t;
    client_.setTransport(t);
    setConnected(true);
    log("Connected: " + t->description());
    readAll();
}

void MainWindow::readAll() {
    for (const auto &p : dev::kParams) client_.requestGet(p.id);
    log("Reading all parameters");
}

void MainWindow::writeAll() {
    for (int r = 0; r < static_cast<int>(dev::kParamCount); ++r) {
        qint32 v = 0;
        if (!cellValue(r, &v)) {
            setStatus(r, "Not written: invalid value", false);
            continue;
        }
        client_.requestSet(dev::kParams[r].id, v);
    }
    log("Writing all parameters");
}

void MainWindow::resetDefaults() {
    for (int r = 0; r < static_cast<int>(dev::kParamCount); ++r) {
        setValueCell(r, dev::kParams[r].def);
        setStatus(r, "", true);
    }
}

bool MainWindow::cellValue(int row, qint32 *value) const {
    bool ok = false;
    const int v = table_->item(row, ColValue)->text().trimmed().toInt(&ok);
    if (!ok) return false;
    const auto &p = dev::kParams[row];
    if (v < p.min || v > p.max) return false;
    if (value) *value = v;
    return true;
}

void MainWindow::onCellChanged(int row, int column) {
    if (column != ColValue) return;
    const bool valid = cellValue(row, nullptr);
    table_->blockSignals(true);
    table_->item(row, ColValue)->setBackground(valid ? QBrush() : QBrush(kBad));
    table_->blockSignals(false);
    setStatus(row, valid ? "Edited (not written)" : "Out of range / not a number", valid);
}

int MainWindow::rowForId(quint8 id) const {
    for (int r = 0; r < static_cast<int>(dev::kParamCount); ++r)
        if (dev::kParams[r].id == id) return r;
    return -1;
}

void MainWindow::setValueCell(int row, qint32 value) {
    table_->blockSignals(true);
    table_->item(row, ColValue)->setText(QString::number(value));
    table_->item(row, ColValue)->setBackground(QBrush());
    table_->blockSignals(false);
}

void MainWindow::setStatus(int row, const QString &text, bool ok) {
    table_->blockSignals(true);
    auto *it = table_->item(row, ColStatus);
    it->setText(text);
    it->setForeground(ok ? QBrush() : QBrush(Qt::red));
    table_->blockSignals(false);
}

void MainWindow::onValue(quint8 id, qint32 value) {
    const int row = rowForId(id);
    if (row < 0) return;
    setValueCell(row, value);
    setStatus(row, "OK (confirmed by device)", true);
}

void MainWindow::onDeviceError(quint8 id, quint8 code) {
    static const char *names[] = {"?", "unknown parameter", "out of range", "bad payload", "unknown command"};
    const QString what = code < 5 ? names[code] : "error";
    const int row = rowForId(id);
    if (row >= 0) setStatus(row, "Device rejected: " + what, false);
    log(QStringLiteral("Device error (id %1): %2").arg(id).arg(what));
}

void MainWindow::onTelemetry(quint32 seq, qint32 reading) {
    telemetryLabel_->setText(QStringLiteral("Telemetry: #%1   reading = %2")
                                 .arg(seq)
                                 .arg(reading / 1000.0, 0, 'f', 3));
}

void MainWindow::exportJson() {
    QJsonObject obj;
    for (int r = 0; r < static_cast<int>(dev::kParamCount); ++r) {
        qint32 v = 0;
        if (!cellValue(r, &v)) {
            QMessageBox::warning(this, "Export", QStringLiteral("Fix invalid value for %1 first.").arg(dev::kParams[r].name));
            return;
        }
        obj[dev::kParams[r].name] = v;
    }
    const QString path = QFileDialog::getSaveFileName(this, "Export settings", "device_config.json", "JSON (*.json)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Export", "Cannot write file: " + f.errorString());
        return;
    }
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    log("Exported to " + path);
}

void MainWindow::importJson() {
    const QString path = QFileDialog::getOpenFileName(this, "Import settings", QString(), "JSON (*.json)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Import", "Cannot read file: " + f.errorString());
        return;
    }
    QJsonParseError perr;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, "Import", "Not a valid JSON object: " + perr.errorString());
        return;
    }
    const auto obj = doc.object();
    int applied = 0;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        int row = -1;
        for (int r = 0; r < static_cast<int>(dev::kParamCount); ++r)
            if (it.key() == dev::kParams[r].name) row = r;
        if (row < 0 || !it.value().isDouble()) {
            log("Import: ignored key " + it.key());
            continue;
        }
        table_->item(row, ColValue)->setText(QString::number(it.value().toInt()));  // triggers validation
        ++applied;
    }
    log(QStringLiteral("Imported %1 values from %2 (use Write all to send)").arg(applied).arg(path));
}

void MainWindow::log(const QString &line) {
    logView_->appendPlainText(QTime::currentTime().toString("HH:mm:ss.zzz") + "  " + line);
}
