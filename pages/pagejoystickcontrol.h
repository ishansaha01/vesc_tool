#ifndef PAGEJOYSTICKCONTROL_H
#define PAGEJOYSTICKCONTROL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QSpinBox>
#include <QTimer>
#include "vescinterface.h"
#include "widgets/directionaljoystick.h"
#include "tcpserversimple.h"

namespace Ui {
class PageJoystickControl;
}

class PageJoystickControl : public QWidget
{
    Q_OBJECT

public:
    explicit PageJoystickControl(QWidget *parent = nullptr);
    ~PageJoystickControl();

    VescInterface *vesc() const;
    void setVesc(VescInterface *vesc);

private slots:
    void onCurrentValueChanged(double value);
    void onDutyValueChanged(double value);
    void onDeadzoneValueChanged(double value);
    void onPrimaryCanIdChanged(int value);
    void onSecondaryCanIdChanged(int value);
    void onResetButtonClicked();
    void onTcpServerToggled(bool checked);
    void onTcpPortChanged(int port);
    void onTcpForwardScaleChanged(double value);
    void onTcpTurnScaleChanged(double value);
    void onTcpDataReceived(const QByteArray &data);
    void onIdealBallSizeChanged(int value);

private:
    Ui::PageJoystickControl *ui;
    VescInterface *mVesc;
    DirectionalJoystick *mJoystick;
    QTimer *mAliveTimer;
    TcpServerSimple *mTcpServer;
    int mTcpPort;
    bool mTcpServerEnabled;
    double mTcpForwardScale;
    double mTcpTurnScale;
    int mIdealBallSize;
    
    void saveSettings();
    void loadSettings();
    void processJoystickData(double forward, double turn);
};

#endif // PAGEJOYSTICKCONTROL_H
