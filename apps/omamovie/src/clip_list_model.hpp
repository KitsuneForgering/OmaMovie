#pragma once

#include <QAbstractListModel>
#include <QVariantList>

// Timeline clips for a QML Repeater without rebuilding every delegate on each edit (M6
// long-form audit: recreating ~0.5 ms of QML per clip made edits on 500 clips take 280-500 ms).
// Rows are the maps Session::clipMap builds; sync() keeps the rows whose clip ID did not move,
// reporting changed values as dataChanged, and removes/inserts only the edited middle.
// One role, "modelData", so delegates read it as they did a QVariantList.
class ClipListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    using QAbstractListModel::QAbstractListModel;

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(rows_.size());
    }
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const { return static_cast<int>(rows_.size()); }

    // O(rows) comparisons; delegates are created only for inserted rows.
    void sync(const QVariantList& rows);

signals:
    void countChanged();

private:
    QVariantList rows_;
};
