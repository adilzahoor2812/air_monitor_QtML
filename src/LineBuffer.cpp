#include "LineBuffer.h"

LineBuffer::FeedResult LineBuffer::feed(const QByteArray &chunk)
{
    FeedResult result;
    if (chunk.isEmpty())
        return result;

    if (chunk.size() > kMaxChunk) {
        result.overflow = true;
        m_pending.clear();
        const qsizetype newline = chunk.lastIndexOf('\n');
        if (newline >= 0) {
            const QByteArray tail = chunk.mid(newline + 1);
            if (tail.size() <= kMaxPending)
                m_pending = tail;
        }
        return result;
    }

    m_pending.append(chunk);
    while (true) {
        const qsizetype newline = m_pending.indexOf('\n');
        if (newline < 0)
            break;
        if (newline > kMaxPending) {
            m_pending.remove(0, newline + 1);
            result.overflow = true;
            continue;
        }
        QByteArray line = m_pending.left(newline);
        m_pending.remove(0, newline + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        result.lines.append(line);
    }

    if (m_pending.size() > kMaxPending) {
        m_pending.clear();
        result.overflow = true;
    }
    return result;
}

void LineBuffer::clear()
{
    m_pending.clear();
}

qsizetype LineBuffer::pendingSize() const
{
    return m_pending.size();
}
