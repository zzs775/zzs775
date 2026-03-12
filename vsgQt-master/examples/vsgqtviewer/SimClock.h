#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <chrono>
#include <rocky/DateTime.h>

class SimClock : public QObject
{
    Q_OBJECT
public:
    explicit SimClock(QObject* parent = nullptr);

    void setTimeRange(const rocky::DateTime& start, const rocky::DateTime& stop);
    void setAnimating(bool anim);
    void setMultiplier(double mult);
    void stop();

    Q_INVOKABLE void seekToProgress(double p);
    Q_INVOKABLE QString timeStringAt(double p) const;
    void syncToDataTime(double dataTimestamp);
    void adjustTimeSpan(double factor);

    rocky::DateTime currentDateTime() const;

    double progress() const;
    double totalSeconds() const;

    // 返回当前仿真秒数（最高精度，通过实时补偿消除抖动）
    double currentSeconds() const;

    bool isAnimating() const;
    double multiplier() const;
    rocky::DateTime startTime() const;
    rocky::DateTime stopTime() const;

signals:
    void timeChanged();
    void rangeChanged();
    void animatingChanged();
    void multiplierChanged();

private slots:
    void onTick();

private:
    QString formatDateTime(const rocky::DateTime& dt) const;

    QTimer* _timer;
    rocky::DateTime _start;
    rocky::DateTime _stop;
    rocky::DateTime _current;
    double _multiplier;
    bool _animating;

    // 高精度计时辅助
    std::chrono::steady_clock::time_point _lastTickTime;
    double _fractionalSeconds;
};
