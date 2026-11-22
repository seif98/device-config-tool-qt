#pragma once
#include "DeviceClient.h"
#include <QMainWindow>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void toggleConnection();
    void readAll();
    void writeAll();
    void resetDefaults();
    void importJson();
    void exportJson();
    void onCellChanged(int row, int column);
    void onValue(quint8 id, qint32 value);
    void onDeviceError(quint8 id, quint8 code);
    void onTelemetry(quint32 seq, qint32 reading);
    void refreshPorts();

private:
    void buildUi();
    void setConnected(bool connected);
    void log(const QString &line);
    int rowForId(quint8 id) const;
    void setValueCell(int row, qint32 value);
    void setStatus(int row, const QString &text, bool ok);
    bool cellValue(int row, qint32 *value) const;

    QComboBox *sourceCombo_ = nullptr;
    QComboBox *portCombo_ = nullptr;
    QComboBox *baudCombo_ = nullptr;
    QPushButton *connectBtn_ = nullptr;
    QPushButton *readBtn_ = nullptr;
    QPushButton *writeBtn_ = nullptr;
    QTableWidget *table_ = nullptr;
    QLabel *telemetryLabel_ = nullptr;
    QPlainTextEdit *logView_ = nullptr;

    DeviceClient client_;
    Transport *transport_ = nullptr;
    int crcErrors_ = 0;
};
