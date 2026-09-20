#include "expressionevaluator.h"

#include <functional>

namespace OVP {
namespace {

class Parser
{
public:
    explicit Parser(const QString &text)
        : m_text(text)
    {
    }

    bool run(double &result)
    {
        skipSpaces();
        if (!parseExpression(result))
            return false;
        skipSpaces();
        return m_pos >= m_text.size();
    }

private:
    void skipSpaces()
    {
        while (m_pos < m_text.size() && m_text.at(m_pos).isSpace())
            ++m_pos;
    }

    bool parseExpression(double &value)
    {
        if (!parseTerm(value))
            return false;
        for (;;) {
            skipSpaces();
            if (m_pos >= m_text.size())
                return true;
            const QChar c = m_text.at(m_pos);
            if (c != QLatin1Char('+') && c != QLatin1Char('-'))
                return true;
            ++m_pos;
            double rhs = 0.0;
            if (!parseTerm(rhs))
                return false;
            value = (c == QLatin1Char('+')) ? value + rhs : value - rhs;
        }
    }

    bool parseTerm(double &value)
    {
        if (!parseFactor(value))
            return false;
        for (;;) {
            skipSpaces();
            if (m_pos >= m_text.size())
                return true;
            const QChar c = m_text.at(m_pos);
            if (c != QLatin1Char('*') && c != QLatin1Char('/'))
                return true;
            ++m_pos;
            double rhs = 0.0;
            if (!parseFactor(rhs))
                return false;
            if (c == QLatin1Char('/')) {
                if (qFuzzyIsNull(rhs))
                    return false;
                value = value / rhs;
            } else {
                value = value * rhs;
            }
        }
    }

    bool parseFactor(double &value)
    {
        skipSpaces();
        if (m_pos >= m_text.size())
            return false;

        const QChar c = m_text.at(m_pos);
        if (c == QLatin1Char('+')) {
            ++m_pos;
            return parseFactor(value);
        }
        if (c == QLatin1Char('-')) {
            ++m_pos;
            double inner = 0.0;
            if (!parseFactor(inner))
                return false;
            value = -inner;
            return true;
        }
        if (c == QLatin1Char('(')) {
            ++m_pos;
            if (!parseExpression(value))
                return false;
            skipSpaces();
            if (m_pos >= m_text.size() || m_text.at(m_pos) != QLatin1Char(')'))
                return false;
            ++m_pos;
            return true;
        }
        return parseNumber(value);
    }

    bool parseNumber(double &value)
    {
        skipSpaces();
        const int start = m_pos;
        bool hasDot = false;
        while (m_pos < m_text.size()) {
            const QChar c = m_text.at(m_pos);
            if (c.isDigit()) {
                ++m_pos;
            } else if (c == QLatin1Char('.') && !hasDot) {
                hasDot = true;
                ++m_pos;
            } else {
                break;
            }
        }
        if (m_pos == start)
            return false;
        const QString token = m_text.mid(start, m_pos - start);
        bool ok = false;
        value = token.toDouble(&ok);
        return ok;
    }

    QString m_text;
    int m_pos = 0;
};

} // namespace

bool evaluateArithmetic(const QString &expression, double &result)
{
    Parser parser(expression);
    return parser.run(result);
}

QString substituteReferences(const QString &line,
                             const std::function<QString(const QString &)> &resolve)
{
    QString out;
    out.reserve(line.size());
    int i = 0;
    while (i < line.size()) {
        const QChar c = line.at(i);
        if (c == QLatin1Char('{')) {
            const int end = line.indexOf(QLatin1Char('}'), i + 1);
            if (end > i) {
                const QString ref = line.mid(i + 1, end - i - 1).trimmed();
                out += resolve(ref);
                i = end + 1;
                continue;
            }
        }
        out += c;
        ++i;
    }
    return out;
}

} // namespace OVP
