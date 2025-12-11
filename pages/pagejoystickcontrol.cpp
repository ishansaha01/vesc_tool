#include "pagejoystickcontrol.h"
#include "ui_pagejoystickcontrol.h"
#include <QSettings>

PageJoystickControl::PageJoystickControl(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageJoystickControl),
    mVesc(nullptr),
    mTcpPort(65102),
    mTcpServerEnabled(false),
    mTcpForwardScale(1.0),
    mTcpTurnScale(1.0),
    mIdealBallSize(45)
{
    ui->setupUi(this);
    
    // Create the joystick widget and add it to the layout
    mJoystick = new DirectionalJoystick(this);
    QVBoxLayout *joystickLayout = new QVBoxLayout(ui->joystickWidget);
    joystickLayout->addWidget(static_cast<QWidget*>(mJoystick));
    
    // Initialize TCP server
    mTcpServer = new TcpServerSimple(this);
    connect(mTcpServer, &TcpServerSimple::dataRx, this, &PageJoystickControl::onTcpDataReceived);
    connect(mTcpServer, &TcpServerSimple::connectionChanged, [this](bool connected, QString address) {
        if (connected) {
            ui->tcpStatusLabel->setText(tr("TCP Status: Connected to %1").arg(address));
        } else {
            ui->tcpStatusLabel->setText(tr("TCP Status: Listening on port %1").arg(mTcpPort));
        }
    });
    
    // Connect signals and slots
    connect(ui->currentBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PageJoystickControl::onCurrentValueChanged);
    connect(ui->dutyBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PageJoystickControl::onDutyValueChanged);
    connect(ui->deadzoneBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PageJoystickControl::onDeadzoneValueChanged);
    connect(ui->primaryCanIdBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PageJoystickControl::onPrimaryCanIdChanged);
    connect(ui->secondaryCanIdBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PageJoystickControl::onSecondaryCanIdChanged);
    connect(ui->resetButton, &QPushButton::clicked, this, &PageJoystickControl::onResetButtonClicked);
    connect(ui->tcpServerEnabledBox, &QCheckBox::toggled, this, &PageJoystickControl::onTcpServerToggled);
    connect(ui->tcpPortBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PageJoystickControl::onTcpPortChanged);
    connect(ui->tcpForwardScaleBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PageJoystickControl::onTcpForwardScaleChanged);
    connect(ui->tcpTurnScaleBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PageJoystickControl::onTcpTurnScaleChanged);
    connect(ui->idealBallSizeBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PageJoystickControl::onIdealBallSizeChanged);
    
    // Set up keep-alive timer
    mAliveTimer = new QTimer(this);
    mAliveTimer->setInterval(200); // 200ms
    connect(mAliveTimer, &QTimer::timeout, [this]() {
        if (mVesc && mVesc->isPortConnected() && ui->keepAliveBox->isChecked()) {
            mVesc->commands()->sendAlive();
        }
    });
    mAliveTimer->start();
    
    loadSettings();
}

PageJoystickControl::~PageJoystickControl()
{
    saveSettings();
    
    if (mAliveTimer) {
        mAliveTimer->stop();
    }
    
    if (mTcpServer) {
        mTcpServer->stopServer();
    }
    
    delete ui;
}

VescInterface *PageJoystickControl::vesc() const
{
    return mVesc;
}

void PageJoystickControl::setVesc(VescInterface *vesc)
{
    mVesc = vesc;
    
    if (mVesc) {
        mJoystick->setVesc(mVesc);
        
        // Apply settings to joystick
        mJoystick->setMaxCurrent(ui->currentBox->value());
        mJoystick->setMaxDuty(ui->dutyBox->value());
        mJoystick->setDeadzone(ui->deadzoneBox->value());
        mJoystick->setCanId(ui->primaryCanIdBox->value());
        mJoystick->setSecondaryCanId(ui->secondaryCanIdBox->value());
    }
}

void PageJoystickControl::onTcpServerToggled(bool checked)
{
    mTcpServerEnabled = checked;
    
    if (checked) {
        // Start TCP server
        if (mTcpServer->startServer(mTcpPort)) {
            ui->tcpStatusLabel->setText(tr("TCP Status: Listening on port %1").arg(mTcpPort));
        } else {
            ui->tcpStatusLabel->setText(tr("TCP Status: Failed to start server on port %1").arg(mTcpPort));
            ui->tcpServerEnabledBox->setChecked(false);
            mTcpServerEnabled = false;
        }
    } else {
        // Stop TCP server
        mTcpServer->stopServer();
        ui->tcpStatusLabel->setText(tr("TCP Status: Disabled"));
    }
}

void PageJoystickControl::onTcpPortChanged(int port)
{
    mTcpPort = port;
    
    // If server is running, restart it with new port
    if (mTcpServerEnabled) {
        mTcpServer->stopServer();
        if (mTcpServer->startServer(mTcpPort)) {
            ui->tcpStatusLabel->setText(tr("TCP Status: Listening on port %1").arg(mTcpPort));
        } else {
            ui->tcpStatusLabel->setText(tr("TCP Status: Failed to start server on port %1").arg(mTcpPort));
            ui->tcpServerEnabledBox->setChecked(false);
            mTcpServerEnabled = false;
        }
    }
}

void PageJoystickControl::onTcpForwardScaleChanged(double value)
{
    mTcpForwardScale = value;
}

void PageJoystickControl::onTcpTurnScaleChanged(double value)
{
    mTcpTurnScale = value;
}

void PageJoystickControl::onIdealBallSizeChanged(int value)
{
    mIdealBallSize = value;
}

void PageJoystickControl::onTcpDataReceived(const QByteArray &data)
{
    // Parse received data
    // Expected format: "JS:forward:turn\n"
    // Where forward and turn are floating point values between -1.0 and 1.0
    
    QString dataStr = QString::fromUtf8(data);
    QStringList lines = dataStr.split("\n", Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        if (line.startsWith("JS:")) {
            QStringList parts = line.mid(3).split(":");
            if (parts.size() >= 2) {
                bool forwardOk = false;
                bool turnOk = false;
                double forward = parts[0].toDouble(&forwardOk);
                double turn = parts[1].toDouble(&turnOk);
                
                if (forwardOk && turnOk) {
                    // For ball tracking: forward is the ball size, turn is the position
                    // Calculate forward based on difference from ideal ball size
                    // If ball is bigger than ideal -> negative (move backward)
                    // If ball is smaller than ideal -> positive (move forward)
                    if (forward > 0) { // Only if we have a valid ball size
                        double sizeDiff = mIdealBallSize - forward;
                        // Normalize to a reasonable range and apply scaling
                        forward = (sizeDiff / mIdealBallSize) / mTcpForwardScale;
                        // Clamp to valid range
                        forward = qBound(-1.0, forward, 1.0);
                    } else {
                        forward = 0.0; // No valid ball size
                    }
                    
                    // Scale and clamp the turn value
                    turn = turn / mTcpTurnScale;
                    turn = qBound(-1.0, turn, 1.0);
                    
                    // Update joystick visualization
                    if (mJoystick) {
                        mJoystick->setExternalControl(forward, turn);
                    } else {
                        // If joystick widget is not available, process directly
                        processJoystickData(forward, turn);
                    }
                }
            }
        }
    }
}

void PageJoystickControl::processJoystickData(double forward, double turn)
{
    if (!mVesc || !mVesc->isPortConnected()) {
        return;
    }
    
    // Calculate left and right motor values based on forward and turn
    double leftMotor = forward - turn;
    double rightMotor = forward + turn;
    
    // Clamp values to [-1, 1]
    leftMotor = qBound(-1.0, leftMotor, 1.0);
    rightMotor = qBound(-1.0, rightMotor, 1.0);
    
    // Get CAN IDs from UI
    int primaryCanId = ui->primaryCanIdBox->value();
    int secondaryCanId = ui->secondaryCanIdBox->value();
    
    // Get max current from UI
    double maxCurrent = ui->currentBox->value();
    
    // Always control both motors
    if (fabs(forward) > 0.1 || fabs(turn) > 0.1) {
        // Use current control for both motors
        if (mVesc) {
            // Store the original CAN settings
            bool wasCan = mVesc->commands()->getSendCan();
            int prevId = mVesc->commands()->getCanSendId();
            
            // Set primary motor
            if (primaryCanId == -1) {
                mVesc->commands()->setSendCan(false);
            } else {
                mVesc->commands()->setSendCan(true, primaryCanId);
            }
            mVesc->commands()->setCurrent(leftMotor * maxCurrent);
            
            // Set secondary motor
            if (secondaryCanId == -1) {
                mVesc->commands()->setSendCan(false);
            } else {
                mVesc->commands()->setSendCan(true, secondaryCanId);
            }
            mVesc->commands()->setCurrent(rightMotor * maxCurrent);
            
            // Restore original CAN settings
            mVesc->commands()->setSendCan(wasCan, prevId);
        }
    } else {
        // Stop motors if joystick is in deadzone
        if (mVesc) {
            // Store the original CAN settings
            bool wasCan = mVesc->commands()->getSendCan();
            int prevId = mVesc->commands()->getCanSendId();
            
            // Set primary motor
            if (primaryCanId == -1) {
                mVesc->commands()->setSendCan(false);
            } else {
                mVesc->commands()->setSendCan(true, primaryCanId);
            }
            mVesc->commands()->setCurrent(0.0);
            
            // Set secondary motor
            if (secondaryCanId == -1) {
                mVesc->commands()->setSendCan(false);
            } else {
                mVesc->commands()->setSendCan(true, secondaryCanId);
            }
            mVesc->commands()->setCurrent(0.0);
            
            // Restore original CAN settings
            mVesc->commands()->setSendCan(wasCan, prevId);
        }
    }
}

void PageJoystickControl::onCurrentValueChanged(double value)
{
    if (mJoystick) {
        mJoystick->setMaxCurrent(value);
    }
}

void PageJoystickControl::onDutyValueChanged(double value)
{
    if (mJoystick) {
        mJoystick->setMaxDuty(value);
    }
}

void PageJoystickControl::onDeadzoneValueChanged(double value)
{
    if (mJoystick) {
        mJoystick->setDeadzone(value);
    }
}

void PageJoystickControl::onPrimaryCanIdChanged(int value)
{
    if (mJoystick) {
        mJoystick->setCanId(value);
    }
}

void PageJoystickControl::onSecondaryCanIdChanged(int value)
{
    if (mJoystick) {
        mJoystick->setSecondaryCanId(value);
    }
}

void PageJoystickControl::onResetButtonClicked()
{
    // Reset joystick settings to defaults
    ui->currentBox->setValue(10.0);
    ui->dutyBox->setValue(0.3);
    ui->deadzoneBox->setValue(0.1);
    ui->primaryCanIdBox->setValue(0);
    ui->secondaryCanIdBox->setValue(70);
    ui->keepAliveBox->setChecked(true);
    ui->tcpServerEnabledBox->setChecked(false);
    ui->tcpPortBox->setValue(65102);
    ui->tcpForwardScaleBox->setValue(1.0);
    ui->tcpTurnScaleBox->setValue(1.0);
    ui->idealBallSizeBox->setValue(45);
}

void PageJoystickControl::saveSettings()
{
    QSettings settings;
    settings.beginGroup("pagejoystickcontrol");
    settings.setValue("max_current", ui->currentBox->value());
    settings.setValue("max_duty", ui->dutyBox->value());
    settings.setValue("deadzone", ui->deadzoneBox->value());
    settings.setValue("primary_can_id", ui->primaryCanIdBox->value());
    settings.setValue("secondary_can_id", ui->secondaryCanIdBox->value());
    settings.setValue("keep_alive", ui->keepAliveBox->isChecked());
    settings.setValue("tcp_server_enabled", ui->tcpServerEnabledBox->isChecked());
    settings.setValue("tcp_port", ui->tcpPortBox->value());
    settings.setValue("tcp_forward_scale", ui->tcpForwardScaleBox->value());
    settings.setValue("tcp_turn_scale", ui->tcpTurnScaleBox->value());
    settings.setValue("ideal_ball_size", ui->idealBallSizeBox->value());
    settings.endGroup();
}

void PageJoystickControl::loadSettings()
{
    QSettings settings;
    settings.beginGroup("pagejoystickcontrol");
    
    if (settings.contains("max_current")) {
        ui->currentBox->setValue(settings.value("max_current").toDouble());
    }
    
    if (settings.contains("max_duty")) {
        ui->dutyBox->setValue(settings.value("max_duty").toDouble());
    }
    
    if (settings.contains("deadzone")) {
        ui->deadzoneBox->setValue(settings.value("deadzone").toDouble());
    }
    
    if (settings.contains("primary_can_id")) {
        ui->primaryCanIdBox->setValue(settings.value("primary_can_id").toInt());
    }
    
    if (settings.contains("secondary_can_id")) {
        ui->secondaryCanIdBox->setValue(settings.value("secondary_can_id").toInt());
    }
    
    if (settings.contains("keep_alive")) {
        ui->keepAliveBox->setChecked(settings.value("keep_alive").toBool());
    }
    
    if (settings.contains("tcp_server_enabled")) {
        ui->tcpServerEnabledBox->setChecked(settings.value("tcp_server_enabled").toBool());
    }
    
    if (settings.contains("tcp_port")) {
        ui->tcpPortBox->setValue(settings.value("tcp_port").toInt());
    }
    
    if (settings.contains("tcp_forward_scale")) {
        ui->tcpForwardScaleBox->setValue(settings.value("tcp_forward_scale").toDouble());
    }
    
    if (settings.contains("tcp_turn_scale")) {
        ui->tcpTurnScaleBox->setValue(settings.value("tcp_turn_scale").toDouble());
    }
    
    if (settings.contains("ideal_ball_size")) {
        ui->idealBallSizeBox->setValue(settings.value("ideal_ball_size").toInt());
    }
    
    settings.endGroup();
    
    // Apply TCP settings
    mTcpPort = ui->tcpPortBox->value();
    mTcpServerEnabled = ui->tcpServerEnabledBox->isChecked();
    mTcpForwardScale = ui->tcpForwardScaleBox->value();
    mTcpTurnScale = ui->tcpTurnScaleBox->value();
    mIdealBallSize = ui->idealBallSizeBox->value();
    
    if (mTcpServerEnabled) {
        onTcpServerToggled(true);
    } else {
        ui->tcpStatusLabel->setText(tr("TCP Status: Disabled"));
    }
}
