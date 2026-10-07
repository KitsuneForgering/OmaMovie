#include "clip_list_model.hpp"

#include <algorithm>

namespace {

constexpr int kRow = Qt::UserRole + 1;

QVariant id_of(const QVariant& row) {
    return row.toMap().value(QStringLiteral("id"));
}

} // namespace

QVariant ClipListModel::data(const QModelIndex& index, int role) const {
    if (role != kRow || !index.isValid() || index.row() >= rows_.size()) return {};
    return rows_[index.row()];
}

QHash<int, QByteArray> ClipListModel::roleNames() const {
    return {{kRow, "modelData"}};
}

void ClipListModel::sync(const QVariantList& rows) {
    const qsizetype old_n = rows_.size();
    const qsizetype new_n = rows.size();
    // The unchanged-by-ID prefix and suffix; an edit touches a range in between.
    qsizetype head = 0;
    while (head < old_n && head < new_n && id_of(rows_[head]) == id_of(rows[head])) ++head;
    qsizetype tail = 0;
    while (tail < old_n - head && tail < new_n - head &&
           id_of(rows_[old_n - 1 - tail]) == id_of(rows[new_n - 1 - tail])) {
        ++tail;
    }
    if (old_n - head - tail > 0) {
        beginRemoveRows({}, static_cast<int>(head), static_cast<int>(old_n - tail - 1));
        rows_.remove(head, old_n - head - tail);
        endRemoveRows();
    }
    if (new_n - head - tail > 0) {
        beginInsertRows({}, static_cast<int>(head), static_cast<int>(new_n - tail - 1));
        for (qsizetype i = head; i < new_n - tail; ++i) rows_.insert(i, rows[i]);
        endInsertRows();
    }
    // Kept rows whose values changed (a ripple moves every later start): one notification per
    // contiguous run.
    qsizetype run = -1;
    const auto flush = [&](qsizetype end) {
        if (run >= 0) emit dataChanged(index(static_cast<int>(run)), index(static_cast<int>(end - 1)), {kRow});
        run = -1;
    };
    for (qsizetype i = 0; i < new_n; ++i) {
        const bool kept = i < head || i >= new_n - tail;
        if (kept && rows_[i] != rows[i]) {
            rows_[i] = rows[i];
            if (run < 0) run = i;
        } else {
            flush(i);
        }
    }
    flush(new_n);
    if (old_n != new_n) emit countChanged();
}
