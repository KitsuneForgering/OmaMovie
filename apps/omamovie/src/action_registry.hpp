#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>

#include <vector>

// The central action system (ui-design §8): ids, names and shortcuts of the QML `OmaAction`s,
// user remapping persisted in an INI file, and the command palette's search. QML declares
// the actions (text, default keys, handler) as properties of one registry object; attach()
// names each action after its property, so ids stay stable without a second list.
class ActionRegistry : public QObject {
    Q_OBJECT
    // Every attached action object, for the one Repeater that registers their shortcuts.
    Q_PROPERTY(QList<QObject*> actions READ actions NOTIFY attached)
public:
    // `settings_file` holds the overrides; the smoke run passes a temporary one.
    explicit ActionRegistry(QString settings_file, QObject* parent = nullptr);

    // Reads every OmaAction-valued property of `registry`, remembers its declared keys as the
    // default and applies saved overrides. Called once, after the registry is complete.
    Q_INVOKABLE void attach(QObject* registry);
    [[nodiscard]] QList<QObject*> actions() const;

    // Assigns `keys` (empty: no shortcut) to action `id` and saves it. Returns "" on success,
    // else why it was refused: an invalid sequence or one another action already uses.
    Q_INVOKABLE QString remap(const QString& id, const QString& keys);
    Q_INVOKABLE void reset(const QString& id);
    Q_INVOKABLE void resetAll();

    // Palette rows {id, text, keys, defaultKeys, enabled}, best matches first: every query
    // word must match the action's name or id as a word prefix (rank 0), a substring (1) or
    // a subsequence (2); an empty query lists every action by name. O(actions × name length).
    Q_INVOKABLE QVariantList search(const QString& query) const;
    // The shortcut a key press spells (Qt::Key and Qt::KeyboardModifiers from a QML KeyEvent),
    // or "" while only modifiers are held.
    Q_INVOKABLE QString sequenceOf(int key, int modifiers) const;
    // Ids of actions sharing one shortcut, "a b" per pair. Qt fires neither of two enabled
    // shortcuts with the same keys, so only actions never enabled together may share one.
    Q_INVOKABLE QStringList conflicts() const;
    // Triggers action `id` if it is enabled; returns whether it ran.
    Q_INVOKABLE bool trigger(const QString& id);

signals:
    void attached();
    void changed();

private:
    struct Entry {
        QString id;
        QPointer<QObject> action;
        QString defaults;
    };
    Entry* find(const QString& id);
    void apply(Entry& entry, const QString& keys);

    QString settings_file_;
    std::vector<Entry> entries_;
};
