#ifndef GUI_REGEX_HIGHLIGHTER_HPP
#define GUI_REGEX_HIGHLIGHTER_HPP

// Syntax highlighting for the regex editors (a QML TextArea's textDocument). The tokens come from
// src/regex_tokens.hpp, which splits a spec the way the engine does.
//
//   TextArea { id: area }
//   RegexHighlighter { document: area.textDocument; mode: RegexHighlighter.Rename; errorPosition: 5 }

#include <QPointer>
#include <QQuickTextDocument>
#include <QSyntaxHighlighter>
#include <QtQml/qqmlregistration.h>

namespace fsturbo::gui {

class SpecHighlighter;

class RegexHighlighter : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickTextDocument* document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(int errorPosition READ errorPosition WRITE setErrorPosition NOTIFY errorPositionChanged)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY darkChanged)

public:
    enum Mode { Rename, Select, Filter };
    Q_ENUM(Mode)

    explicit RegexHighlighter(QObject* parent = nullptr);

    QQuickTextDocument* document() const { return document_; }
    void setDocument(QQuickTextDocument* document);
    Mode mode() const { return mode_; }
    void setMode(Mode mode);
    // UTF-16 index of the character to underline as the engine's error (-1: none).
    int errorPosition() const { return error_position_; }
    void setErrorPosition(int position);
    bool dark() const { return dark_; }
    void setDark(bool dark);

Q_SIGNALS:
    void documentChanged();
    void modeChanged();
    void errorPositionChanged();
    void darkChanged();

private:
    void rehighlight();

    QPointer<QQuickTextDocument> document_;
    SpecHighlighter* highlighter_ = nullptr;
    Mode mode_ = Rename;
    int error_position_ = -1;
    bool dark_ = false;
};

} // namespace fsturbo::gui

#endif // GUI_REGEX_HIGHLIGHTER_HPP
