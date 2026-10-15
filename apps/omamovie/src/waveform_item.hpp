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
    Q_PROPERTY(
        double from READ from WRITE setFrom NOTIFY changed) // media time (s) at the left edge
    Q_PROPERTY(
        double length READ length WRITE setLength NOTIFY changed) // media seconds across the width
    Q_PROPERTY(double gain READ gain WRITE setGain NOTIFY changed)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY changed)
    // The timeline's scroll: items wider than wideWidth bind it and build only the bars near
    // the screen (waveform_span.hpp); narrower ones leave it alone and build every bar.
    Q_PROPERTY(double scroll READ scroll WRITE setScroll NOTIFY changed)
    Q_PROPERTY(double wideWidth READ wideWidth CONSTANT)

public:
    explicit WaveformItem(QQuickItem* parent = nullptr);

    [[nodiscard]] QObject* store() const;
    void setStore(QObject* store);
    [[nodiscard]] double media() const;
    void setMedia(double media);
    [[nodiscard]] double from() const;
    void setFrom(double from);
    [[nodiscard]] double length() const;
    void setLength(double length);
    [[nodiscard]] double gain() const;
    void setGain(double gain);
    [[nodiscard]] QColor color() const;
    void setColor(const QColor& color);
    [[nodiscard]] double scroll() const;
    void setScroll(double scroll);
    [[nodiscard]] double wideWidth() const;

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
    double scroll_ = 0;
    std::shared_ptr<const oma::playback::Waveform> waveform_;
};
