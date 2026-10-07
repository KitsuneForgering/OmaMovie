#include "action_registry.hpp"

#include <QKeySequence>
#include <QMetaProperty>
#include <QSettings>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <limits>
#include <utility>

namespace {

constexpr auto kGroup = "shortcuts";

// One canonical spelling, so "ctrl+b" and "Ctrl+B" are the same shortcut and conflicts compare
// equal. An empty result for non-empty input means Qt could not parse it.
QString canonical(const QString& keys) {
    return QKeySequence(keys).toString(QKeySequence::PortableText);
}

// 0: a word of `text` starts with `word`; 1: `text` contains it; 2: its letters appear in
// order; -1: no match. Both are lower case.
int rank(const QString& text, const QString& word) {
    const auto at = text.indexOf(word);
    if (at >= 0) {
        for (auto i = at; i >= 0; i = text.indexOf(word, i + 1)) {
            if (i == 0 || !text[i - 1].isLetterOrNumber()) return 0;
        }
        return 1;
    }
    qsizetype next = 0;
    for (const QChar c : word) {
        next = text.indexOf(c, next);
        if (next < 0) return -1;
        ++next;
    }
    return 2;
}

} // namespace

ActionRegistry::ActionRegistry(QString settings_file, QObject* parent)
    : QObject(parent), settings_file_(std::move(settings_file)) {}

void ActionRegistry::attach(QObject* registry) {
    entries_.clear();
    if (registry == nullptr) return;
    const QSettings settings(settings_file_, QSettings::IniFormat);
    const QMetaObject* meta = registry->metaObject();
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
        auto* action = meta->property(i).read(registry).value<QObject*>();
        // An OmaAction is the only kind of object in the registry with a string `keys`.
        if (action == nullptr || action->property("keys").metaType().id() != QMetaType::QString) {
            continue;
        }
        Entry entry{.id = QString::fromLatin1(meta->property(i).name()), .action = action,
                    .defaults = action->property("keys").toString()};
        const QString key = QStringLiteral("%1/%2").arg(QLatin1String(kGroup), entry.id);
        if (settings.contains(key)) apply(entry, settings.value(key).toString());
        entries_.push_back(std::move(entry));
    }
    emit attached();
    emit changed();
}

QList<QObject*> ActionRegistry::actions() const {
    QList<QObject*> list;
    list.reserve(static_cast<qsizetype>(entries_.size()));
    for (const Entry& entry : entries_) {
        if (entry.action) list.append(entry.action.data());
    }
    return list;
}

ActionRegistry::Entry* ActionRegistry::find(const QString& id) {
    const auto it = std::ranges::find(entries_, id, &Entry::id);
    return it == entries_.end() || !it->action ? nullptr : &*it;
}

void ActionRegistry::apply(Entry& entry, const QString& keys) {
    if (entry.action) entry.action->setProperty("keys", keys);
}

QString ActionRegistry::remap(const QString& id, const QString& keys) {
    Entry* entry = find(id);
    if (entry == nullptr) return QStringLiteral("Unknown action");
    const QString wanted = canonical(keys);
    if (!keys.trimmed().isEmpty() && wanted.isEmpty()) return QStringLiteral("Not a valid shortcut");
    if (!wanted.isEmpty()) {
        for (const Entry& other : entries_) {
            if (&other == entry || !other.action) continue;
            if (canonical(other.action->property("keys").toString()) == wanted) {
                return QStringLiteral("Already used by “%1”").arg(other.action->property("text").toString());
            }
        }
    }
    apply(*entry, wanted);
    QSettings settings(settings_file_, QSettings::IniFormat);
    const QString key = QStringLiteral("%1/%2").arg(QLatin1String(kGroup), id);
    if (wanted == canonical(entry->defaults)) {
        settings.remove(key);
    } else {
        settings.setValue(key, wanted);
    }
    emit changed();
    return {};
}

void ActionRegistry::reset(const QString& id) {
    Entry* entry = find(id);
    if (entry == nullptr) return;
    apply(*entry, entry->defaults);
    QSettings settings(settings_file_, QSettings::IniFormat);
    settings.remove(QStringLiteral("%1/%2").arg(QLatin1String(kGroup), id));
    emit changed();
}

void ActionRegistry::resetAll() {
    for (Entry& entry : entries_) apply(entry, entry.defaults);
    QSettings settings(settings_file_, QSettings::IniFormat);
    settings.remove(QLatin1String(kGroup));
    emit changed();
}

QVariantList ActionRegistry::search(const QString& query) const {
    const QStringList words = query.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    struct Hit {
        int score;
        const Entry* entry;
        QString text;
    };
    std::vector<Hit> hits;
    hits.reserve(entries_.size());
    for (const Entry& entry : entries_) {
        if (!entry.action) continue;
        QString text = entry.action->property("text").toString();
        const QString name = text.toLower();
        const QString id = entry.id.toLower();
        int score = 0;
        for (const QString& word : words) {
            const int by_name = rank(name, word);
            const int by_id = rank(id, word);
            const int best = by_name < 0 ? by_id : by_id < 0 ? by_name : std::min(by_name, by_id);
            if (best < 0) {
                score = std::numeric_limits<int>::max();
                break;
            }
            score += best;
        }
        if (score != std::numeric_limits<int>::max()) hits.push_back({score, &entry, std::move(text)});
    }
    std::ranges::stable_sort(hits, [](const Hit& a, const Hit& b) {
        return a.score != b.score ? a.score < b.score : a.text.localeAwareCompare(b.text) < 0;
    });
    QVariantList rows;
    rows.reserve(static_cast<qsizetype>(hits.size()));
    for (const Hit& hit : hits) {
        rows.append(QVariantMap{{QStringLiteral("id"), hit.entry->id},
                                {QStringLiteral("text"), hit.text},
                                {QStringLiteral("keys"), hit.entry->action->property("keys")},
                                {QStringLiteral("defaultKeys"), hit.entry->defaults},
                                {QStringLiteral("enabled"), hit.entry->action->property("enabled")}});
    }
    return rows;
}

QString ActionRegistry::sequenceOf(int key, int modifiers) const {
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_Meta:
    case Qt::Key_AltGr:
    case Qt::Key_unknown:
        return {};
    default:
        break;
    }
    // Keypad is a location, not a different shortcut.
    const auto mods = Qt::KeyboardModifiers::fromInt(modifiers) & ~Qt::KeypadModifier;
    return QKeySequence(QKeyCombination(mods, static_cast<Qt::Key>(key))).toString(QKeySequence::PortableText);
}

QStringList ActionRegistry::conflicts() const {
    QStringList pairs;
    // O(actions²) over ~50 actions, run by the smoke check, not per key press.
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (!entries_[i].action) continue;
        const QString keys = canonical(entries_[i].action->property("keys").toString());
        if (keys.isEmpty()) continue;
        for (std::size_t j = i + 1; j < entries_.size(); ++j) {
            if (entries_[j].action && canonical(entries_[j].action->property("keys").toString()) == keys) {
                pairs.append(entries_[i].id + QLatin1Char(' ') + entries_[j].id);
            }
        }
    }
    return pairs;
}

bool ActionRegistry::trigger(const QString& id) {
    Entry* entry = find(id);
    if (entry == nullptr || !entry->action->property("enabled").toBool()) return false;
    return QMetaObject::invokeMethod(entry->action, "trigger");
}
