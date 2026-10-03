#pragma once

#include "oma/playback/waveform.hpp"

#include <QColor>
#include <QPointer>
#include <QQuickItem>

#include <memory>

class WaveformStore;

// Draws a clip's waveform (ui-design §7.2) as scene-graph geometry: one bar per two pixels over
// the media range the clip shows, mirrored around the middle, on a decibel scale so speech stays
// visible. The heights follow the clip's gain.
//
// Threading: properties change on the UI thread; updatePaintNode runs on the render thread while
// the UI thread is blocked, and only reads the immutable waveform.
class WaveformItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject* store READ store WRITE setStore NOTIFY changed)
    Q_PROPERTY(double media READ media WRITE setMedia NOTIFY changed)
    Q_PROPERTY(double from READ from WRITE setFrom NOTIFY changed)     // media time (s) at the left edge
    Q_PROPERTY(double length READ length WRITE setLength NOTIFY changed) // media seconds across the width
    Q_PROPERTY(double gain READ gain WRITE setGain NOTIFY changed)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY changed)

public:
    explicit WaveformItem(QQuickItem* parent = nullptr);

    [[nodiscard]] QObject* store() const;
    void setStore(QObject* store);
    [[nodiscard]] double media() const { return media_; }
    void setMedia(double media);
    [[nodiscard]] double from() const { return from_; }
    void setFrom(double from);
    [[nodiscard]] double length() const { return length_; }
    void setLength(double length);
    [[nodiscard]] double gain() const { return gain_; }
    void setGain(double gain);
    [[nodiscard]] QColor color() const { return color_; }
    void setColor(const QColor& color);

signals:
    void changed();

protected:
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData* data) override;
    void geometryChange(const QRectF& now, const QRectF& before) override;

private:
    void refresh();

    QPointer<WaveformStore> store_;
    double media_ = 0;
    double from_ = 0;
    double length_ = 0;
    double gain_ = 1;
    QColor color_ = Qt::white;
    std::shared_ptr<const oma::playback::Waveform> waveform_;
};
