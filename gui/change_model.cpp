#include "change_model.hpp"

namespace fsturbo::gui {

QVariant ChangeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= count()) return {};
    const ChangeRow& row = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
        case KindRole:
            switch (row.kind) {
                case ChangeRow::Kind::Flatten: return QStringLiteral("flatten");
                case ChangeRow::Kind::File: return QStringLiteral("file");
                case ChangeRow::Kind::Directory: return QStringLiteral("dir");
                case ChangeRow::Kind::Error: return QStringLiteral("error");
            }
            return {};
        case SourceRole: return row.source;
        case DestRole: return row.dest;
        default: return {};
    }
}

QHash<int, QByteArray> ChangeModel::roleNames() const {
    return {{KindRole, "kind"}, {SourceRole, "source"}, {DestRole, "dest"}};
}

void ChangeModel::append(std::vector<ChangeRow> rows) {
    if (rows.empty()) return;
    beginInsertRows({}, count(), count() + static_cast<int>(rows.size()) - 1);
    rows_.insert(rows_.end(), std::make_move_iterator(rows.begin()), std::make_move_iterator(rows.end()));
    endInsertRows();
    Q_EMIT countChanged();
}

void ChangeModel::clear() {
    if (rows_.empty()) return;
    beginResetModel();
    rows_.clear();
    endResetModel();
    Q_EMIT countChanged();
}

} // namespace fsturbo::gui
