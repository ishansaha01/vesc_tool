#include "pagejoystickcontrol.h"
#include "ui_pagejoystickcontrol.h"
#include <QSettings>

PageJoystickControl::PageJoystickControl(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageJoystickControl),
    mVesc(nullptr)
{
    ui->setupUi(this);
    
    // Create the joystick widget and add it to the layout
    mJoystick = new DirectionalJoystick(this);
    QVBoxLayout *joystickLayout = new QVBoxLayout(ui->joystickWidget);
    joystickLayout->addWidget(mJoystick);
    
    // Connect signals and slots
    connect(ui->currentBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PageJoystickControl::onCurrentValueChanged);
    connect(ui->dutyBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PageJoystickControl::onDutyValueChanged);
    connect(ui->deadzoneBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PageJoystickControl::onDeadzoneValueChanged);
    connect(ui->primaryCanIdBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PageJoystickControl::onPrimaryCanIdChanged);
    connect(ui->secondaryCanIdBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PageJoystickControl::onSecondaryCanIdChanged);
    connect(ui->resetButton, &QPushButton::clicked, this, &PageJoystickControl::onResetButtonClicked);
    
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
    
    settings.endGroup();
}
