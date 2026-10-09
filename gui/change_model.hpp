#ifndef GUI_CHANGE_MODEL_HPP
#define GUI_CHANGE_MODEL_HPP

// The rows of the GUI's change list: what a run renamed or flattened (or would, in a preview),
// and the errors, in the order the engine reported them.

#include <vector>
#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

namespace fsturbo::gui {

struct ChangeRow {
    enum class Kind : quint8 { Flatten, File, Directory, Error };
    Kind kind = Kind::File;
    QString source;  // the entry's path relative to the target (Error: the message)
    QString dest;    // its new name
};

class ChangeModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("owned by TransformController")
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { KindRole = Qt::UserRole + 1, SourceRole, DestRole };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : static_cast<int>(rows_.size()); }
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(rows_.size()); }
    void append(std::vector<ChangeRow> rows);
    void clear();

Q_SIGNALS:
    void countChanged();

private:
    std::vector<ChangeRow> rows_;
};

} // namespace fsturbo::gui

#endif // GUI_CHANGE_MODEL_HPP
