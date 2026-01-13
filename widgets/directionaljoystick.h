#ifndef DIRECTIONALJOYSTICK_H
#define DIRECTIONALJOYSTICK_H

#include <QWidget>
#include <QTimer>
#include <QPointF>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QColor>
#include <QElapsedTimer>
#include "vescinterface.h"

class DirectionalJoystick : public QWidget
{
    Q_OBJECT

public:
    explicit DirectionalJoystick(QWidget *parent = nullptr);
    ~DirectionalJoystick();

    void setVesc(VescInterface *vesc);
    void setCanId(int canId) { mCanId = canId; }
    void setSecondaryCanId(int canId) { mSecondaryCanId = canId; }
    void setMaxCurrent(double current) { mMaxCurrent = current; }
    void setMaxDuty(double duty) { mMaxDuty = duty; }
    void setDeadzone(double deadzone) { mDeadzone = deadzone; }
    
    // Set joystick position from external source
    void setExternalControl(double forward, double turn);
    bool isExternallyControlled() const { return mExternalControl; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void timerSlot();

private:
    VescInterface *mVesc;
    QTimer mTimer;
    QPointF mJoystickPos;
    QPointF mJoystickCenter;
    double mJoystickRadius;
    bool mJoystickActive;
    int mCanId;
    int mSecondaryCanId;
    double mMaxCurrent;
    double mMaxDuty;
    double mDeadzone;
    double mLastForward;
    double mLastTurn;
    bool mExternalControl;
    QElapsedTimer mExternalControlTimeout;
    
    void updateJoystickPositionFromMouse(const QPoint &mousePos);
    void normalizeJoystickPosition();
    void updateMotors();
    void setMotorCurrent(int canId, double current);
    void setMotorDuty(int canId, double duty);
    void stopMotors();
};

#endif // DIRECTIONALJOYSTICK_H
