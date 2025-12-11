#include "directionaljoystick.h"
#include <QPainter>
#include <QBrush>
#include <QPen>
#include <QFontMetrics>
#include <cmath>

DirectionalJoystick::DirectionalJoystick(QWidget *parent) : QWidget(parent)
{
    mVesc = nullptr;
    mJoystickActive = false;
    mCanId = 0;
    mSecondaryCanId = 70; // Default to 70, same as WASD control
    mMaxCurrent = 10.0;
    mMaxDuty = 0.3;
    mDeadzone = 0.1;
    mLastForward = 0.0;
    mLastTurn = 0.0;
    
    // Set up timer for joystick updates
    mTimer.setInterval(50); // 50ms interval for smooth control
    connect(&mTimer, &QTimer::timeout, this, &DirectionalJoystick::timerSlot);
    
    // Set minimum size
    setMinimumSize(200, 200);
    
    // Set focus policy to enable keyboard focus
    setFocusPolicy(Qt::StrongFocus);
    
    // Set mouse tracking to capture mouse movements
    setMouseTracking(true);
}

DirectionalJoystick::~DirectionalJoystick()
{
    if (mTimer.isActive()) {
        mTimer.stop();
        stopMotors();
    }
}

void DirectionalJoystick::setVesc(VescInterface *vesc)
{
    mVesc = vesc;
}

void DirectionalJoystick::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Calculate dimensions
    int w = width();
    int h = height();
    int size = qMin(w, h);
    mJoystickRadius = size * 0.4;
    mJoystickCenter = QPointF(w / 2, h / 2);
    
    // If joystick is not active, center it
    if (!mJoystickActive) {
        mJoystickPos = mJoystickCenter;
    }
    
    // Draw outer circle (joystick boundary)
    painter.setPen(QPen(Qt::darkGray, 2));
    painter.setBrush(QBrush(QColor(240, 240, 240)));
    painter.drawEllipse(mJoystickCenter, mJoystickRadius, mJoystickRadius);
    
    // Draw deadzone circle
    painter.setPen(QPen(Qt::lightGray, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(mJoystickCenter, mJoystickRadius * mDeadzone, mJoystickRadius * mDeadzone);
    
    // Draw crosshairs
    painter.setPen(QPen(Qt::lightGray, 1));
    painter.drawLine(QPointF(w/2, h/2 - mJoystickRadius), QPointF(w/2, h/2 + mJoystickRadius));
    painter.drawLine(QPointF(w/2 - mJoystickRadius, h/2), QPointF(w/2 + mJoystickRadius, h/2));
    
    // Draw joystick handle
    QRadialGradient gradient(mJoystickPos, mJoystickRadius * 0.3);
    gradient.setColorAt(0, QColor(60, 60, 200));
    gradient.setColorAt(1, QColor(30, 30, 150));
    painter.setPen(QPen(Qt::black, 1));
    painter.setBrush(gradient);
    painter.drawEllipse(mJoystickPos, mJoystickRadius * 0.3, mJoystickRadius * 0.3);
    
    // Draw labels
    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);
    
    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance("Forward");
    painter.drawText(QPointF(w/2 - textWidth/2, h/2 - mJoystickRadius - 10), "Forward");
    
    textWidth = fm.horizontalAdvance("Backward");
    painter.drawText(QPointF(w/2 - textWidth/2, h/2 + mJoystickRadius + 20), "Backward");
    
    textWidth = fm.horizontalAdvance("Left");
    painter.drawText(QPointF(w/2 - mJoystickRadius - textWidth - 10, h/2), "Left");
    
    textWidth = fm.horizontalAdvance("Right");
    painter.drawText(QPointF(w/2 + mJoystickRadius + 10, h/2), "Right");
    
    // Draw current values
    QString forwardText = QString("Forward: %1%").arg(mLastForward * 100.0, 0, 'f', 0);
    QString turnText = QString("Turn: %1%").arg(mLastTurn * 100.0, 0, 'f', 0);
    
    painter.drawText(QPointF(10, h - 30), forwardText);
    painter.drawText(QPointF(10, h - 10), turnText);
}

void DirectionalJoystick::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        mJoystickActive = true;
        updateJoystickPositionFromMouse(event->pos());
        
        if (!mTimer.isActive() && mVesc && mVesc->isPortConnected()) {
            mTimer.start();
        }
        
        update();
    }
}

void DirectionalJoystick::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        mJoystickActive = false;
        mJoystickPos = mJoystickCenter;
        
        if (mTimer.isActive()) {
            mTimer.stop();
            stopMotors();
        }
        
        update();
    }
}

void DirectionalJoystick::mouseMoveEvent(QMouseEvent *event)
{
    if (mJoystickActive) {
        updateJoystickPositionFromMouse(event->pos());
        update();
    }
}

void DirectionalJoystick::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    
    // Recalculate joystick dimensions
    int w = width();
    int h = height();
    mJoystickCenter = QPointF(w / 2, h / 2);
    
    if (!mJoystickActive) {
        mJoystickPos = mJoystickCenter;
    }
}

void DirectionalJoystick::timerSlot()
{
    if (mVesc && mVesc->isPortConnected() && mJoystickActive) {
        updateMotors();
    }
}

void DirectionalJoystick::updateJoystickPositionFromMouse(const QPoint &mousePos)
{
    // Calculate vector from center to mouse position
    QPointF delta = mousePos - mJoystickCenter;
    
    // Calculate distance from center
    double distance = sqrt(delta.x() * delta.x() + delta.y() * delta.y());
    
    // If distance is greater than radius, normalize to radius
    if (distance > mJoystickRadius) {
        delta *= (mJoystickRadius / distance);
    }
    
    // Update joystick position
    mJoystickPos = mJoystickCenter + delta;
    
    // Update motors based on new position
    if (mVesc && mVesc->isPortConnected() && mTimer.isActive()) {
        updateMotors();
    }
}

void DirectionalJoystick::normalizeJoystickPosition()
{
    // Calculate normalized joystick position (-1 to 1 in both axes)
    double dx = (mJoystickPos.x() - mJoystickCenter.x()) / mJoystickRadius;
    double dy = (mJoystickPos.y() - mJoystickCenter.y()) / mJoystickRadius;
    
    // Apply deadzone
    if (fabs(dx) < mDeadzone) {
        dx = 0.0;
    } else {
        // Rescale the range to still use the full output range
        dx = (dx - (dx > 0 ? mDeadzone : -mDeadzone)) / (1.0 - mDeadzone);
    }
    
    if (fabs(dy) < mDeadzone) {
        dy = 0.0;
    } else {
        // Rescale the range to still use the full output range
        dy = (dy - (dy > 0 ? mDeadzone : -mDeadzone)) / (1.0 - mDeadzone);
    }
    
    // Invert Y-axis for intuitive control (up is forward)
    mLastForward = -dy;
    mLastTurn = dx;
}

void DirectionalJoystick::updateMotors()
{
    if (!mVesc || !mVesc->isPortConnected()) {
        return;
    }
    
    // Normalize joystick position to get forward and turn values
    normalizeJoystickPosition();
    
    // Calculate left and right motor values based on forward and turn
    double leftMotor = mLastForward - mLastTurn;
    double rightMotor = mLastForward + mLastTurn;
    
    // Clamp values to [-1, 1]
    leftMotor = qBound(-1.0, leftMotor, 1.0);
    rightMotor = qBound(-1.0, rightMotor, 1.0);
    
    // Always control both motors
    if (fabs(mLastForward) > 0.1 || fabs(mLastTurn) > 0.1) {
        // Use current control for both motors
        setMotorCurrent(mCanId, leftMotor * mMaxCurrent);
        setMotorCurrent(mSecondaryCanId, rightMotor * mMaxCurrent);
    } else {
        // Stop motors if joystick is in deadzone
        stopMotors();
    }
}

void DirectionalJoystick::setMotorCurrent(int canId, double current)
{
    if (!mVesc || !mVesc->isPortConnected()) {
        return;
    }
    
    // Store the original CAN settings
    bool wasCan = mVesc->commands()->getSendCan();
    int prevId = mVesc->commands()->getCanSendId();
    
    // Special case for CAN ID -1 (direct motor control without CAN)
    if (canId == -1) {
        // Disable CAN to control the directly connected motor
        mVesc->commands()->setSendCan(false);
        mVesc->commands()->setCurrent(current);
    } else {
        // For all other CAN IDs
        // Set CAN ID for the specific motor
        mVesc->commands()->setSendCan(true, canId);
        
        // Set the current
        mVesc->commands()->setCurrent(current);
    }
    
    // Restore original CAN settings
    mVesc->commands()->setSendCan(wasCan, prevId);
}

void DirectionalJoystick::setMotorDuty(int canId, double duty)
{
    if (!mVesc || !mVesc->isPortConnected()) {
        return;
    }
    
    // Store the original CAN settings
    bool wasCan = mVesc->commands()->getSendCan();
    int prevId = mVesc->commands()->getCanSendId();
    
    // Special case for CAN ID -1 (direct motor control without CAN)
    if (canId == -1) {
        // Disable CAN to control the directly connected motor
        mVesc->commands()->setSendCan(false);
        mVesc->commands()->setDutyCycle(duty);
    } else {
        // For all other CAN IDs
        // Set CAN ID for the specific motor
        mVesc->commands()->setSendCan(true, canId);
        
        // Set the duty cycle
        mVesc->commands()->setDutyCycle(duty);
    }
    
    // Restore original CAN settings
    mVesc->commands()->setSendCan(wasCan, prevId);
}

void DirectionalJoystick::stopMotors()
{
    if (!mVesc || !mVesc->isPortConnected()) {
        return;
    }
    
    // Stop both motors
    setMotorCurrent(mCanId, 0.0);
    setMotorCurrent(mSecondaryCanId, 0.0);
    
    mLastForward = 0.0;
    mLastTurn = 0.0;
}
