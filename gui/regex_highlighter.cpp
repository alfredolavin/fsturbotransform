#include "regex_highlighter.hpp"

#include <array>
#include <vector>
#include <QTextDocument>
#include "regex_tokens.hpp"

namespace fsturbo::gui {

using syntax::Token;

class SpecHighlighter final : public QSyntaxHighlighter {
public:
    using QSyntaxHighlighter::QSyntaxHighlighter;

    syntax::Mode mode = syntax::Mode::Rename;
    int error_position = -1;
    bool dark = false;

protected:
    void highlightBlock(const QString& text) override {
        // The tokenizer works on UTF-8 bytes; the document on UTF-16 code units.
        const QByteArray utf8 = text.toUtf8();
        std::vector<int> unit_at_byte;
        unit_at_byte.reserve(static_cast<std::size_t>(utf8.size()) + 1);
        for (qsizetype i = 0; i < text.size(); ++i) {
            char32_t cp = text[i].unicode();
            const bool pair = QChar::isHighSurrogate(cp) && i + 1 < text.size() && text[i + 1].isLowSurrogate();
            if (pair) cp = QChar::surrogateToUcs4(text[i], text[i + 1]);
            const int bytes = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
            for (int b = 0; b < bytes; ++b) unit_at_byte.push_back(static_cast<int>(i));
            if (pair) ++i;
        }
        unit_at_byte.push_back(static_cast<int>(text.size()));

        for (const syntax::Span& span : syntax::tokenize(std::string_view(utf8.constData(), static_cast<std::size_t>(utf8.size())), mode)) {
            const int start = unit_at_byte[span.start];
            int length = unit_at_byte[span.start + span.length] - start;
            if (length == 0 && span.token == Token::Error) length = 1; // a missing character: mark the next one
            // Merged, so a token keeps the background of the block it sits in (an expression's tint).
            const QTextCharFormat f = format(span);
            for (int k = start; k < start + length && k < text.size(); ++k) {
                QTextCharFormat merged = QSyntaxHighlighter::format(k);
                merged.merge(f);
                setFormat(k, 1, merged);
            }
        }

        const int at = error_position - currentBlock().position();
        if (error_position >= 0 && at >= 0 && at <= text.size() && !text.isEmpty()) {
            const int pos = std::min(at, static_cast<int>(text.size()) - 1);
            QTextCharFormat f = QSyntaxHighlighter::format(pos);
            f.setUnderlineStyle(QTextCharFormat::WaveUnderline);
            f.setUnderlineColor(colour(Token::Error));
            f.setBackground(QColor(dark ? "#5a1d24" : "#ffd9dc"));
            setFormat(pos, 1, f);
        }
    }

private:
    QColor colour(Token token, int depth = 0) const {
        // Light / dark pairs, chosen to stay readable on both Breeze backgrounds.
        static constexpr std::array<std::array<const char*, 2>, 4> kGroups = {{
            {"#1d6fd0", "#5cb1ff"}, {"#8e44ad", "#d39bf0"}, {"#0f8a6f", "#43d6b0"}, {"#b9620b", "#f6a85a"},
        }};
        const auto pick = [&](const char* light, const char* dark_hex) { return QColor(dark ? dark_hex : light); };
        switch (token) {
            case Token::Delimiter: return pick("#7f8c8d", "#95a5a6");
            case Token::Flag: return pick("#2e7d32", "#7ddc82");
            case Token::IgnoredFlag: return pick("#9aa5a6", "#6c7a7b");
            case Token::Escape: return pick("#c2185b", "#ff7fb0");
            case Token::Class: return pick("#00838f", "#4dd8e5");
            case Token::Group: return pick(kGroups[depth % 4][0], kGroups[depth % 4][1]);
            case Token::GroupName: return pick("#6a1b9a", "#e0a6ff");
            case Token::Arrow: return pick("#d35400", "#ffab5e");
            case Token::Quantifier: return pick("#d35400", "#ffab5e");
            case Token::Anchor: return pick("#b71c1c", "#ff8a80");
            case Token::Alternation: return pick("#b71c1c", "#ff8a80");
            case Token::Dot: return pick("#c2185b", "#ff7fb0");
            case Token::Comment: return pick("#8a9597", "#7f8c8d");
            case Token::GroupRef: return pick("#1565c0", "#82b8ff");
            case Token::CaseOp: return pick("#00897b", "#64ffda");
            case Token::ExprHead: return pick("#6a1b9a", "#e0a6ff");
            case Token::ExprBrace: return pick("#6a1b9a", "#e0a6ff");
            case Token::JsNumber: return pick("#e65100", "#ffcb6b");
            case Token::JsString: return pick("#2a9d3a", "#c3e88d");
            case Token::JsVariable: return pick("#1565c0", "#89ddff");
            case Token::JsFunction: return pick("#00897b", "#80cbc4");
            case Token::JsOperator: return pick("#c2185b", "#c792ea");
            case Token::Glob: return pick("#d35400", "#ffab5e");
            case Token::Error: return pick("#d32f2f", "#ff5370");
            case Token::Expression: return {};
        }
        return {};
    }

    QTextCharFormat format(const syntax::Span& span) const {
        QTextCharFormat f;
        if (span.token == Token::Expression) {
            f.setBackground(QColor(dark ? "#2d2440" : "#f3ecfa"));
            return f;
        }
        f.setForeground(colour(span.token, span.depth));
        switch (span.token) {
            case Token::Delimiter:
            case Token::Group:
            case Token::Arrow:
            case Token::ExprHead:
            case Token::ExprBrace:
            case Token::Flag:
                f.setFontWeight(QFont::Bold);
                break;
            case Token::Comment:
            case Token::IgnoredFlag:
                f.setFontItalic(true);
                break;
            case Token::Class:
                f.setBackground(QColor(dark ? "#123a40" : "#e0f5f7"));
                break;
            case Token::Error:
                f.setUnderlineStyle(QTextCharFormat::WaveUnderline);
                f.setUnderlineColor(colour(Token::Error));
                break;
            default:
                break;
        }
        return f;
    }
};

RegexHighlighter::RegexHighlighter(QObject* parent) : QObject(parent) {}

void RegexHighlighter::setDocument(QQuickTextDocument* document) {
    if (document_ == document) return;
    document_ = document;
    delete highlighter_;
    highlighter_ = nullptr;
    if (document_ != nullptr) {
        highlighter_ = new SpecHighlighter(document_->textDocument());
        highlighter_->setParent(this);
        rehighlight();
    }
    Q_EMIT documentChanged();
}

void RegexHighlighter::setMode(Mode mode) {
    if (mode_ == mode) return;
    mode_ = mode;
    rehighlight();
    Q_EMIT modeChanged();
}

void RegexHighlighter::setErrorPosition(int position) {
    if (error_position_ == position) return;
    error_position_ = position;
    rehighlight();
    Q_EMIT errorPositionChanged();
}

void RegexHighlighter::setDark(bool dark) {
    if (dark_ == dark) return;
    dark_ = dark;
    rehighlight();
    Q_EMIT darkChanged();
}

void RegexHighlighter::rehighlight() {
    if (highlighter_ == nullptr) return;
    highlighter_->mode = mode_ == Rename ? syntax::Mode::Rename : mode_ == Select ? syntax::Mode::Select : syntax::Mode::Filter;
    highlighter_->error_position = error_position_;
    highlighter_->dark = dark_;
    highlighter_->rehighlight();
}

} // namespace fsturbo::gui
