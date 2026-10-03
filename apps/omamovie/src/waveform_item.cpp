#include "waveform_item.hpp"

#include "waveform_store.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

// Bars are drawn on a decibel scale from this floor: quiet speech still shows, silence does not.
constexpr double kFloorDb = -54.0;
constexpr double kBarStep = 2.0; // pixels per bar

double bar_height(float peak, double gain) {
    const double v = static_cast<double>(peak) * gain;
    if (v <= 0.0) return 0.0;
    const double db = 20.0 * std::log10(v);
    return std::clamp((db - kFloorDb) / -kFloorDb, 0.0, 1.0);
}

} // namespace

WaveformItem::WaveformItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents);
}

QObject* WaveformItem::store() const {
    return store_.data();
}

void WaveformItem::setStore(QObject* store) {
    auto* typed = qobject_cast<WaveformStore*>(store);
    if (typed == store_) return;
    if (store_) disconnect(store_, nullptr, this, nullptr);
    store_ = typed;
    if (store_) {
        connect(store_, &WaveformStore::ready, this, [this](double media) {
            if (media == media_) refresh();
        });
    }
    refresh();
}

void WaveformItem::setMedia(double media) {
    if (media == media_) return;
    media_ = media;
    refresh();
}

void WaveformItem::setFrom(double from) {
    if (from == from_) return;
    from_ = from;
    emit changed();
    update();
}

void WaveformItem::setLength(double length) {
    if (length == length_) return;
    length_ = length;
    emit changed();
    update();
}

void WaveformItem::setGain(double gain) {
    if (gain == gain_) return;
    gain_ = gain;
    emit changed();
    update();
}

void WaveformItem::setColor(const QColor& color) {
    if (color == color_) return;
    color_ = color;
    emit changed();
    update();
}

void WaveformItem::refresh() {
    waveform_ = store_ && media_ > 0 ? store_->get(static_cast<std::uint64_t>(media_)) : nullptr;
    emit changed();
    update();
}

void WaveformItem::geometryChange(const QRectF& now, const QRectF& before) {
    QQuickItem::geometryChange(now, before);
    if (now.size() != before.size()) update();
}

QSGNode* WaveformItem::updatePaintNode(QSGNode* old, UpdatePaintNodeData* /*data*/) {
    const double w = width();
    const double h = height();
    if (!waveform_ || w < 1.0 || h < 2.0 || length_ <= 0.0) {
        delete old;
        return nullptr;
    }
    auto* node = static_cast<QSGGeometryNode*>(old);
    if (node == nullptr) {
        node = new QSGGeometryNode;
        auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new QSGFlatColorMaterial);
        node->setFlag(QSGNode::OwnsMaterial);
    }
    auto* material = static_cast<QSGFlatColorMaterial*>(node->material());
    if (material->color() != color_) {
        material->setColor(color_);
        node->markDirty(QSGNode::DirtyMaterial);
    }
    const int bars = static_cast<int>(std::ceil(w / kBarStep));
    QSGGeometry* geometry = node->geometry();
    geometry->allocate(bars * 6);
    QSGGeometry::Point2D* v = geometry->vertexDataAsPoint2D();
    const double seconds_per_bar = length_ / static_cast<double>(bars);
    const double origin = from_ - waveform_->start.seconds_approx(); // the waveform's own clock
    const double middle = h / 2.0;
    for (int i = 0; i < bars; ++i) {
        const double t = origin + (static_cast<double>(i) * seconds_per_bar);
        const double half = std::max(0.5, bar_height(waveform_->peak_between(t, t + seconds_per_bar), gain_) * middle);
        const auto x0 = static_cast<float>(static_cast<double>(i) * kBarStep);
        const auto x1 = static_cast<float>(std::min(w, x0 + kBarStep - 0.5));
        const auto y0 = static_cast<float>(middle - half);
        const auto y1 = static_cast<float>(middle + half);
        v[(i * 6) + 0].set(x0, y0);
        v[(i * 6) + 1].set(x1, y0);
        v[(i * 6) + 2].set(x0, y1);
        v[(i * 6) + 3].set(x1, y0);
        v[(i * 6) + 4].set(x1, y1);
        v[(i * 6) + 5].set(x0, y1);
    }
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
