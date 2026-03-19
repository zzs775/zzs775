#include "SimClock.h"
#include <ctime>
#include <iomanip>
#include <sstream>

SimClock::SimClock(QObject* parent) : QObject(parent),
                                      _multiplier(1.0),
                                      _animating(false),
                                      _fractionalSeconds(0.0)
{
    _timer = new QTimer(this);
    _timer->setInterval(33); // 仅用于 UI 通知（timeChanged）
    connect(_timer, &QTimer::timeout, this, &SimClock::onTick);
}

void SimClock::setTimeRange(const rocky::DateTime& start, const rocky::DateTime& stop)
{
    // 如果起止时间完全没变，直接忽略
    if (_start.asTimeStamp() == start.asTimeStamp() && _stop.asTimeStamp() == stop.asTimeStamp())
    {
        return;
    }

    // 如果仅仅是终点向后扩展（流式缓冲），我们不应该重置当前播放位置 _current
    bool justExpanding = (_start.asTimeStamp() == start.asTimeStamp() && stop.asTimeStamp() > _stop.asTimeStamp());

    _start = start;
    _stop = stop;

    if (!justExpanding && _current.asTimeStamp() > _stop.asTimeStamp())
    {
        // 如果不是正常的向后扩展，或者当前光标已经跑出界了，才重置
        _current = start;
        _fractionalSeconds = 0.0;
    }

    emit rangeChanged();
    emit timeChanged();
}

void SimClock::setAnimating(bool anim)
{
    _animating = anim;
    if (_animating)
    {
        _lastTickTime = std::chrono::steady_clock::now();
        _timer->start();
    }
    else
    {
        _timer->stop();
    }
    emit animatingChanged();
}

void SimClock::setMultiplier(double mult)
{
    if (_multiplier == mult) return;
    _multiplier = mult;
    emit multiplierChanged();
}

void SimClock::stop()
{
    setAnimating(false);
    _current = _start;
    _fractionalSeconds = 0.0;
    emit timeChanged();
}

void SimClock::tickFixed(double dt)
{
    if (!_animating) return;

    // 【核心：固定步长时间推进】
    // dt 是渲染循环的固定间隔（如 0.008 秒），乘以倍速得到仿真推进量
    double simDt = dt * _multiplier;
    _fractionalSeconds += simDt;

    // 当积累的小数秒超过 1 秒时，同步更新 rocky::DateTime 整数位
    if (_fractionalSeconds >= 1.0 || _fractionalSeconds <= -1.0)
    {
        rocky::TimeStamp intSeconds = static_cast<rocky::TimeStamp>(_fractionalSeconds);
        _fractionalSeconds -= intSeconds;
        _current = rocky::DateTime(_current.asTimeStamp() + intSeconds);

        // 边界检查：防止跑过头
        if (_current.asTimeStamp() > _stop.asTimeStamp())
        {
            _current = _stop;
            _fractionalSeconds = 0.0;
            setAnimating(false);
        }
        else if (_current.asTimeStamp() < _start.asTimeStamp())
        {
            _current = _start;
            _fractionalSeconds = 0.0;
            setAnimating(false);
        }
    }
}

void SimClock::seekToProgress(double p)
{
    p = std::max(0.0, std::min(1.0, p));
    double total = totalSeconds();
    double offset = total * p;
    _current = rocky::DateTime(_start.asTimeStamp() + static_cast<rocky::TimeStamp>(offset));
    _fractionalSeconds = offset - std::floor(offset);
    emit timeChanged();
}

void SimClock::syncToDataTime(double dataTimestamp)
{
    double offset = dataTimestamp;
    _current = rocky::DateTime(_start.asTimeStamp() + static_cast<rocky::TimeStamp>(offset));
    _fractionalSeconds = offset - std::floor(offset);
    emit timeChanged();
}

void SimClock::adjustTimeSpan(double factor)
{
    // 如果没有持续时间，不能扩缩
    double total = totalSeconds();
    if (total <= 0.0) return;

    double newTotal = std::max(10.0, total * factor);
    // 可选：保证缩放后不能短于当前已播放的时间，留出 1 秒余量
    if (newTotal < currentSeconds() + 1.0)
    {
        newTotal = currentSeconds() + 1.0;
    }

    rocky::TimeStamp intSeconds = static_cast<rocky::TimeStamp>(newTotal);
    _stop = rocky::DateTime(_start.asTimeStamp() + intSeconds);

    emit rangeChanged();
    emit timeChanged();
}

QString SimClock::timeStringAt(double p) const
{
    p = std::max(0.0, std::min(1.0, p));
    double total = totalSeconds();
    rocky::DateTime dt(_start.asTimeStamp() + static_cast<rocky::TimeStamp>(total * p));
    return formatDateTime(dt);
}

rocky::DateTime SimClock::currentDateTime() const
{
    return _current;
}

double SimClock::progress() const
{
    double total = totalSeconds();
    if (total <= 0.0) return 0.0;
    return currentSeconds() / total;
}

double SimClock::totalSeconds() const
{
    return static_cast<double>(_stop.asTimeStamp() - _start.asTimeStamp());
}

double SimClock::currentSeconds() const
{
    // 【固定步长模式】直接返回累积时间，不再做壁钟补偿
    // 这样每次渲染循环调用时，看到的时间永远是稳定的、一致的、不会跳变的
    double base = static_cast<double>(_current.asTimeStamp() - _start.asTimeStamp()) + _fractionalSeconds;
    return base;
}

bool SimClock::isAnimating() const
{
    return _animating;
}

double SimClock::multiplier() const
{
    return _multiplier;
}

rocky::DateTime SimClock::startTime() const
{
    return _start;
}

rocky::DateTime SimClock::stopTime() const
{
    return _stop;
}

void SimClock::onTick()
{
    // 【固定步长模式】onTick 仅作为 UI 信号通知，不再推进时间
    // 时间推进完全由 tickFixed() 接管，从渲染循环主动调用
    if (!_animating) return;
    emit timeChanged();
}

QString SimClock::formatDateTime(const rocky::DateTime& dt) const
{
    auto ts = dt.asTimeStamp();
    std::time_t time = static_cast<std::time_t>(ts);
    std::tm* tm = std::gmtime(&time);
    if (!tm) return "";
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%b %d %Y %H:%M:%S UTC", tm);
    return QString::fromStdString(buffer);
}
