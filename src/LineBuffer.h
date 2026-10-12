#pragma once

#include <QByteArray>
#include <QList>

class LineBuffer
{
public:
    struct FeedResult
    {
        QList<QByteArray> lines;
        bool overflow = false;
    };

    FeedResult feed(const QByteArray &chunk);
    void clear();
    qsizetype pendingSize() const;

private:
    static constexpr qsizetype kMaxPending = 4096;
    static constexpr qsizetype kMaxChunk = 65536;

    QByteArray m_pending;
};
