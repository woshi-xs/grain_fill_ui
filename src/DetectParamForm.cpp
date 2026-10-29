#include "DetectParamForm.h"
#include "ui_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QSpinBox>

namespace {

struct IntField
{
    int grain_filling::DetectParam::*member;
    QSpinBox *Ui::Widget::*box;
};

struct DoubleField
{
    double grain_filling::DetectParam::*member;
    QDoubleSpinBox *Ui::Widget::*box;
};

struct ComboField
{
    int grain_filling::DetectParam::*member;
    QComboBox *Ui::Widget::*box;
};

const IntField kIntFields[] = {
    {&grain_filling::DetectParam::minArea, &Ui::Widget::minAreaSpin},
    {&grain_filling::DetectParam::maxArea, &Ui::Widget::maxAreaSpin},
};

const DoubleField kDoubleFields[] = {
    {&grain_filling::DetectParam::areaRatio, &Ui::Widget::areaRatioSpin},
    {&grain_filling::DetectParam::lengthRatio, &Ui::Widget::lengthRatioSpin},
    {&grain_filling::DetectParam::widthRatio, &Ui::Widget::widthRatioSpin},
};

const ComboField kComboFields[] = {
    {&grain_filling::DetectParam::connectivity, &Ui::Widget::connectivityCombo},
    {&grain_filling::DetectParam::labelType, &Ui::Widget::labelTypeCombo},
};

}

void DetectParamForm::initOptions(Ui::Widget *ui)
{
    ui->connectivityCombo->clear();
    ui->connectivityCombo->addItem(QStringLiteral("4邻接"), 4);
    ui->connectivityCombo->addItem(QStringLiteral("8邻接"), 8);

    ui->labelTypeCombo->clear();
    ui->labelTypeCombo->addItem(QStringLiteral("CV_32S"), CV_32S);
    ui->labelTypeCombo->addItem(QStringLiteral("CV_16U"), CV_16U);
}

void DetectParamForm::write(Ui::Widget *ui, const grain_filling::DetectParam &param)
{
    {
        const QSignalBlocker block(ui->binaryThresholdSpin);
        ui->binaryThresholdSpin->setValue(static_cast<int>(param.thresh));
    }
    for (const IntField &field : kIntFields)
    {
        const QSignalBlocker block(ui->*field.box);
        (ui->*field.box)->setValue(param.*field.member);
    }
    for (const DoubleField &field : kDoubleFields)
    {
        const QSignalBlocker block(ui->*field.box);
        (ui->*field.box)->setValue(param.*field.member);
    }
    for (const ComboField &field : kComboFields)
    {
        const QSignalBlocker block(ui->*field.box);
        const int index = (ui->*field.box)->findData(param.*field.member);
        (ui->*field.box)->setCurrentIndex(index < 0 ? 0 : index);
    }
    {
        const QSignalBlocker block(ui->roundFillCountCheck);
        ui->roundFillCountCheck->setChecked(param.roundFillCount);
    }
}

grain_filling::DetectParam DetectParamForm::read(const Ui::Widget *ui)
{
    grain_filling::DetectParam param;
    param.thresh = ui->binaryThresholdSpin->value();
    for (const IntField &field : kIntFields)
        param.*field.member = (ui->*field.box)->value();
    for (const DoubleField &field : kDoubleFields)
        param.*field.member = (ui->*field.box)->value();
    for (const ComboField &field : kComboFields)
        param.*field.member = (ui->*field.box)->currentData().toInt();
    param.roundFillCount = ui->roundFillCountCheck->isChecked();
    return param;
}

bool DetectParamForm::thresholdChanged(const grain_filling::DetectParam &current, const grain_filling::DetectParam &applied)
{
    return static_cast<int>(current.thresh) != static_cast<int>(applied.thresh);
}

bool DetectParamForm::otherChanged(const grain_filling::DetectParam &current, const grain_filling::DetectParam &applied)
{
    for (const IntField &field : kIntFields)
    {
        if (current.*field.member != applied.*field.member)
            return true;
    }
    for (const DoubleField &field : kDoubleFields)
    {
        if (current.*field.member != applied.*field.member)
            return true;
    }
    for (const ComboField &field : kComboFields)
    {
        if (current.*field.member != applied.*field.member)
            return true;
    }
    return current.roundFillCount != applied.roundFillCount;
}

void DetectParamForm::bindChanges(Ui::Widget *ui, QObject *context, const std::function<void()> &onChanged)
{
    const auto notifyInt = [onChanged](int) { onChanged(); };
    const auto notifyDouble = [onChanged](double) { onChanged(); };
    QObject::connect(ui->binaryThresholdSpin, qOverload<int>(&QSpinBox::valueChanged), context, notifyInt);
    for (const IntField &field : kIntFields)
        QObject::connect(ui->*field.box, qOverload<int>(&QSpinBox::valueChanged), context, notifyInt);
    for (const DoubleField &field : kDoubleFields)
        QObject::connect(ui->*field.box, qOverload<double>(&QDoubleSpinBox::valueChanged), context, notifyDouble);
    for (const ComboField &field : kComboFields)
        QObject::connect(ui->*field.box, qOverload<int>(&QComboBox::currentIndexChanged), context, notifyInt);
    QObject::connect(ui->roundFillCountCheck, &QCheckBox::toggled, context, [onChanged](bool) { onChanged(); });
}
