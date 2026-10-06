#include "vajra/gui/canvas/schematic_view.hpp"

#include <QScrollBar>
#include <QMenu>
#include <QContextMenuEvent>
#include <cmath>

namespace vajra::gui {

SchematicView::SchematicView(QWidget* parent)
    : QGraphicsView(parent) {
    setup_viewport();
}

SchematicView::SchematicView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent) {
    setup_viewport();
}

void SchematicView::setup_viewport() {
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setDragMode(QGraphicsView::RubberBandDrag);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
    viewport()->grabGesture(Qt::PinchGesture);

    update_cursor();
}

void SchematicView::update_cursor() {
    if (is_panning_) {
        viewport()->setCursor(Qt::ClosedHandCursor);
    } else if (tool_mode_ == CanvasToolMode::HAND || spacebar_pressed_) {
        viewport()->setCursor(Qt::OpenHandCursor);
    } else {
        viewport()->setCursor(Qt::ArrowCursor);
    }
}

void SchematicView::ensure_scene_rect_covers_viewport() {
    if (!scene()) return;

    QRectF visible_scene = mapToScene(viewport()->rect()).boundingRect();
    QRectF cur_rect = scene()->sceneRect();
    QRectF padded_visible = visible_scene.adjusted(-400.0, -400.0, 400.0, 400.0);

    if (!cur_rect.contains(padded_visible)) {
        scene()->setSceneRect(cur_rect.united(padded_visible));
    }
}

void SchematicView::set_tool_mode(CanvasToolMode mode) {
    if (tool_mode_ == mode) return;
    tool_mode_ = mode;
    if (tool_mode_ == CanvasToolMode::HAND) {
        setDragMode(QGraphicsView::NoDrag);
    } else {
        setDragMode(QGraphicsView::RubberBandDrag);
    }
    update_cursor();
    emit tool_mode_changed(tool_mode_);
}

void SchematicView::toggle_tool_mode() {
    set_tool_mode(tool_mode_ == CanvasToolMode::SELECT ? CanvasToolMode::HAND : CanvasToolMode::SELECT);
}

void SchematicView::zoom_in() {
    apply_zoom_at(1.2, viewport()->rect().center());
}

void SchematicView::zoom_out() {
    apply_zoom_at(1.0 / 1.2, viewport()->rect().center());
}

void SchematicView::reset_zoom() {
    if (current_zoom_ > 0.0) {
        apply_zoom_at(1.0 / current_zoom_, viewport()->rect().center());
    }
}

void SchematicView::apply_zoom_at(double factor, const QPointF& viewport_pos) {
    if (factor <= 0.0) return;

    double target_zoom = current_zoom_ * factor;
    if (target_zoom < 0.01) {
        factor = 0.01 / current_zoom_;
        target_zoom = 0.01;
    } else if (target_zoom > 100.0) {
        factor = 100.0 / current_zoom_;
        target_zoom = 100.0;
    }

    if (std::abs(factor - 1.0) < 1e-4) {
        return;
    }

    QPointF scene_anchor_before = mapToScene(viewport_pos.toPoint());

    scale(factor, factor);
    current_zoom_ = target_zoom;

    QPointF scene_anchor_after = mapToScene(viewport_pos.toPoint());
    QPointF current_center = mapToScene(viewport()->rect().center());
    QPointF new_center = current_center - (scene_anchor_after - scene_anchor_before);
    centerOn(new_center);

    ensure_scene_rect_covers_viewport();

    emit zoom_changed(current_zoom_);
}

void SchematicView::fit_to_all(double margin) {
    if (!scene()) return;

    QRectF bounds = scene()->itemsBoundingRect();
    if (bounds.isEmpty() || bounds.width() <= 0 || bounds.height() <= 0) {
        bounds = QRectF(-200, -200, 400, 400);
    }

    bounds.adjust(-margin, -margin, margin, margin);
    fitInView(bounds, Qt::KeepAspectRatio);

    ensure_scene_rect_covers_viewport();

    current_zoom_ = transform().m11();
    emit zoom_changed(current_zoom_);
}

bool SchematicView::viewportEvent(QEvent* event) {
    if (event->type() == QEvent::Gesture) {
        if (handle_gesture_event(static_cast<QGestureEvent*>(event))) {
            return true;
        }
    } else if (event->type() == QEvent::NativeGesture) {
        if (handle_native_gesture_event(static_cast<QNativeGestureEvent*>(event))) {
            return true;
        }
    } else if (event->type() == QEvent::TouchBegin ||
               event->type() == QEvent::TouchUpdate ||
               event->type() == QEvent::TouchEnd) {
        if (handle_touch_event(static_cast<QTouchEvent*>(event))) {
            return true;
        }
    }
    return QGraphicsView::viewportEvent(event);
}

bool SchematicView::handle_gesture_event(QGestureEvent* event) {
    if (QGesture* pinch = event->gesture(Qt::PinchGesture)) {
        handle_pinch_gesture(static_cast<QPinchGesture*>(pinch));
        event->accept();
        return true;
    }
    return false;
}

void SchematicView::handle_pinch_gesture(QPinchGesture* gesture) {
    QPinchGesture::ChangeFlags change_flags = gesture->changeFlags();

    // Pan delta from pinch center movement
    if (change_flags & QPinchGesture::CenterPointChanged) {
        QPointF center_delta = gesture->centerPoint() - gesture->lastCenterPoint();
        if (std::abs(center_delta.x()) > 0.5 || std::abs(center_delta.y()) > 0.5) {
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - std::round(center_delta.x()));
            verticalScrollBar()->setValue(verticalScrollBar()->value() - std::round(center_delta.y()));
            ensure_scene_rect_covers_viewport();
        }
    }

    // Zoom scale factor
    if (change_flags & QPinchGesture::ScaleFactorChanged) {
        qreal factor = gesture->scaleFactor();
        if (factor > 0.05 && factor < 20.0) {
            QPointF center_screen = gesture->centerPoint();
            QPointF center_viewport = viewport()->mapFromGlobal(center_screen.toPoint());
            if (!viewport()->rect().contains(center_viewport.toPoint())) {
                center_viewport = viewport()->rect().center();
            }
            apply_zoom_at(factor, center_viewport);
        }
    }
}

bool SchematicView::handle_native_gesture_event(QNativeGestureEvent* event) {
    if (event->gestureType() == Qt::ZoomNativeGesture) {
        qreal delta = event->value();
        qreal factor = 1.0 + delta;
        if (factor > 0.1 && factor < 10.0) {
            QPointF viewport_pos = event->pos();
            apply_zoom_at(factor, viewport_pos);
        }
        event->accept();
        return true;
    }
    return false;
}

bool SchematicView::handle_touch_event(QTouchEvent* event) {
    const auto& points = event->touchPoints();
    if (points.size() == 2) {
        const auto& p0 = points[0];
        const auto& p1 = points[1];

        QPointF cur_center = (p0.pos() + p1.pos()) * 0.5;
        QPointF last_center = (p0.lastPos() + p1.lastPos()) * 0.5;

        // Two-finger pan translation
        QPointF pan_delta = cur_center - last_center;
        if (std::abs(pan_delta.x()) > 0.3 || std::abs(pan_delta.y()) > 0.3) {
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - std::round(pan_delta.x()));
            verticalScrollBar()->setValue(verticalScrollBar()->value() - std::round(pan_delta.y()));
            ensure_scene_rect_covers_viewport();
        }

        // Pinch zoom factor
        qreal cur_dist = QLineF(p0.pos(), p1.pos()).length();
        qreal last_dist = QLineF(p0.lastPos(), p1.lastPos()).length();

        if (last_dist > 5.0 && cur_dist > 5.0) {
            qreal factor = cur_dist / last_dist;
            if (factor > 0.6 && factor < 1.6 && std::abs(factor - 1.0) > 0.005) {
                apply_zoom_at(factor, cur_center);
            }
        }

        event->accept();
        return true;
    } else if (points.size() == 1) {
        if (tool_mode_ == CanvasToolMode::HAND || spacebar_pressed_) {
            const auto& p0 = points[0];
            if (event->type() == QEvent::TouchBegin) {
                last_touch_pos_ = p0.pos().toPoint();
            } else if (event->type() == QEvent::TouchUpdate) {
                QPoint delta = p0.pos().toPoint() - last_touch_pos_;
                last_touch_pos_ = p0.pos().toPoint();
                horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
                verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
                ensure_scene_rect_covers_viewport();
            }
            event->accept();
            return true;
        }
    }
    return false;
}

void SchematicView::wheelEvent(QWheelEvent* event) {
    // 1. Ctrl + Wheel: Smooth Zoom centered at cursor
    if (event->modifiers() & Qt::ControlModifier) {
        double factor = 1.0;
        if (!event->pixelDelta().isNull()) {
            factor = 1.0 + event->pixelDelta().y() * 0.005;
        } else {
            int degrees = event->angleDelta().y() / 8;
            int steps = degrees / 15;
            if (steps != 0) {
                factor = (steps > 0) ? std::pow(1.15, steps) : std::pow(1.0 / 1.15, -steps);
            } else if (event->angleDelta().y() != 0) {
                factor = std::pow(1.0015, event->angleDelta().y());
            }
        }

        if (std::abs(factor - 1.0) > 0.001) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            apply_zoom_at(factor, event->position());
#else
            apply_zoom_at(factor, event->posF());
#endif
        }
        event->accept();
        return;
    }

    // 2. Standard Wheel / Two-finger Touchpad: Smooth 2D Panning
    QPoint pixel_delta = event->pixelDelta();
    QPoint angle_delta = event->angleDelta();

    int dx = 0;
    int dy = 0;

    if (!pixel_delta.isNull()) {
        // High-precision pixel delta from touchpad
        dx = pixel_delta.x();
        dy = pixel_delta.y();
    } else if (!angle_delta.isNull()) {
        // Mouse wheel or stepped touchpad
        dx = angle_delta.x() / 2;
        dy = angle_delta.y() / 2;
    }

    if (event->modifiers() & Qt::ShiftModifier) {
        // Shift forces horizontal panning
        if (dx == 0 && dy != 0) {
            dx = dy;
            dy = 0;
        }
    }

    if (dx != 0 || dy != 0) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - dx);
        verticalScrollBar()->setValue(verticalScrollBar()->value() - dy);
        ensure_scene_rect_covers_viewport();
        event->accept();
        return;
    }

    event->accept();
}

void SchematicView::mousePressEvent(QMouseEvent* event) {
    bool should_pan = (event->button() == Qt::MiddleButton) ||
                      (event->button() == Qt::LeftButton && (tool_mode_ == CanvasToolMode::HAND || spacebar_pressed_ || (event->modifiers() & Qt::AltModifier)));

    if (should_pan) {
        is_panning_ = true;
        last_mouse_pos_ = event->pos();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void SchematicView::mouseMoveEvent(QMouseEvent* event) {
    emit mouse_moved_scene(mapToScene(event->pos()));

    if (is_panning_) {
        QPoint delta = event->pos() - last_mouse_pos_;
        last_mouse_pos_ = event->pos();

        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        ensure_scene_rect_covers_viewport();
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void SchematicView::mouseReleaseEvent(QMouseEvent* event) {
    if (is_panning_) {
        is_panning_ = false;
        update_cursor();
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void SchematicView::mouseDoubleClickEvent(QMouseEvent* event) {
    // If double clicking on an item (module box, gate, etc.), let the item handle drill down
    QGraphicsItem* item = itemAt(event->pos());
    if (item) {
        QGraphicsView::mouseDoubleClickEvent(event);
        return;
    }

    // Double click on empty canvas: Fit All
    if (event->button() == Qt::LeftButton) {
        fit_to_all();
        event->accept();
        return;
    }

    QGraphicsView::mouseDoubleClickEvent(event);
}

void SchematicView::keyPressEvent(QKeyEvent* event) {
    // Spacebar engages temporary Hand / Pan mode
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        spacebar_pressed_ = true;
        update_cursor();
        event->accept();
        return;
    }
    // Mode switching keys
    if (event->key() == Qt::Key_H) {
        set_tool_mode(CanvasToolMode::HAND);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_S || event->key() == Qt::Key_Escape) {
        set_tool_mode(CanvasToolMode::SELECT);
        event->accept();
        return;
    }
    // Arrow key panning
    int pan_step = (event->modifiers() & Qt::ShiftModifier) ? 180 : 60;
    if (event->key() == Qt::Key_Left) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - pan_step);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Right) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + pan_step);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Up) {
        verticalScrollBar()->setValue(verticalScrollBar()->value() - pan_step);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Down) {
        verticalScrollBar()->setValue(verticalScrollBar()->value() + pan_step);
        event->accept();
        return;
    }
    // Zoom & navigation keys
    if (event->key() == Qt::Key_F) {
        fit_to_all();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        zoom_in();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Minus || event->key() == Qt::Key_Underscore) {
        zoom_out();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_0) {
        reset_zoom();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_U || event->key() == Qt::Key_Backspace) {
        emit ascend_requested();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Home) {
        emit reset_to_top_requested();
        event->accept();
        return;
    }

    QGraphicsView::keyPressEvent(event);
}

void SchematicView::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        spacebar_pressed_ = false;
        if (is_panning_ && tool_mode_ == CanvasToolMode::SELECT) {
            is_panning_ = false;
        }
        update_cursor();
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void SchematicView::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    auto* act_export = menu.addAction("Export Schematic...");
    connect(act_export, &QAction::triggered, this, &SchematicView::export_requested);

    menu.addSeparator();
    auto* act_fit = menu.addAction("Fit to View (F)");
    connect(act_fit, &QAction::triggered, this, [this]() { fit_to_all(); });

    auto* act_reset = menu.addAction("Reset Zoom (100%)");
    connect(act_reset, &QAction::triggered, this, [this]() { reset_zoom(); });

    menu.addSeparator();
    auto* act_sel = menu.addAction("Select Mode (S)");
    act_sel->setCheckable(true);
    act_sel->setChecked(tool_mode_ == CanvasToolMode::SELECT);
    connect(act_sel, &QAction::triggered, this, [this]() { set_tool_mode(CanvasToolMode::SELECT); });

    auto* act_hand = menu.addAction("Hand Mode (H)");
    act_hand->setCheckable(true);
    act_hand->setChecked(tool_mode_ == CanvasToolMode::HAND);
    connect(act_hand, &QAction::triggered, this, [this]() { set_tool_mode(CanvasToolMode::HAND); });

    menu.exec(event->globalPos());
}

} // namespace vajra::gui
