#include "pingresult.h"

QString PingResult::statusText() const
{
    if (dnsFailed) return QStringLiteral("解析失败");
    if (isRunning) return QStringLiteral("探测中");
    if (isOnline) return QStringLiteral("在线");
    return QStringLiteral("离线");
}

QString PingResult::elapsedText() const
{
    if (startTimeMs <= 0) return QStringLiteral("00:00:00");
    qint64 totalSeconds = elapsedMs / 1000;
    int hours   = static_cast<int>(totalSeconds / 3600);
    int minutes = static_cast<int>((totalSeconds % 3600) / 60);
    int seconds = static_cast<int>(totalSeconds % 60);
    return QStringLiteral("%1:%2:%3")
        .arg(hours,   2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QVariant PingResult::toVariant() const
{
    QVariantMap map;
    map[QStringLiteral("index")]         = index;
    map[QStringLiteral("originalInput")] = originalInput;
    map[QStringLiteral("targetIp")]      = targetIp;
    map[QStringLiteral("resolvedDomain")]= resolvedDomain;
    map[QStringLiteral("hostname")]= hostname;
    map[QStringLiteral("sent")]          = sent;
    map[QStringLiteral("received")]      = received;
    map[QStringLiteral("lost")]         = lost;
    map[QStringLiteral("lossRate")]      = lossRate;
    map[QStringLiteral("minRttUs")]     = minRttUs;
    map[QStringLiteral("maxRttUs")]     = maxRttUs;
    map[QStringLiteral("avgRttUs")]     = avgRttUs;
    map[QStringLiteral("lastRttUs")]   = lastRttUs;
    map[QStringLiteral("isOnline")]     = isOnline;
    map[QStringLiteral("isRunning")]   = isRunning;
    map[QStringLiteral("dnsFailed")]    = dnsFailed;
    map[QStringLiteral("startTimeMs")]  = startTimeMs;
    map[QStringLiteral("elapsedMs")]   = elapsedMs;
    return map;
}

PingResult PingResult::fromVariant(const QVariant& v)
{
    PingResult result;
    QVariantMap map = v.toMap();
    result.index          = map.value(QStringLiteral("index"), 0).toInt();
    result.originalInput  = map.value(QStringLiteral("originalInput")).toString();
    result.targetIp       = map.value(QStringLiteral("targetIp")).toString();
    result.resolvedDomain= map.value(QStringLiteral("resolvedDomain")).toString();
    result.hostname      = map.value(QStringLiteral("hostname")).toString();
    result.sent          = map.value(QStringLiteral("sent"), 0).toInt();
    result.received      = map.value(QStringLiteral("received"), 0).toInt();
    result.lost          = map.value(QStringLiteral("lost"), 0).toInt();
    result.lossRate      = map.value(QStringLiteral("lossRate"), 0.0).toDouble();
    result.minRttUs      = map.value(QStringLiteral("minRttUs"), 0).toLongLong();
    result.maxRttUs      = map.value(QStringLiteral("maxRttUs"), 0).toLongLong();
    result.avgRttUs      = map.value(QStringLiteral("avgRttUs"), 0).toLongLong();
    result.lastRttUs     = map.value(QStringLiteral("lastRttUs"), 0).toLongLong();
    result.isOnline      = map.value(QStringLiteral("isOnline"), false).toBool();
    result.isRunning     = map.value(QStringLiteral("isRunning"), false).toBool();
    result.dnsFailed     = map.value(QStringLiteral("dnsFailed"), false).toBool();
    result.startTimeMs   = map.value(QStringLiteral("startTimeMs"), 0).toLongLong();
    result.elapsedMs     = map.value(QStringLiteral("elapsedMs"), 0).toLongLong();
    return result;
}
