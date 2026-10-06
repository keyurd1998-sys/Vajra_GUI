#pragma once

#include <QGraphicsView>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QEvent>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QNativeGestureEvent>
#include <QTouchEvent>
#include <QPoint>
#include <QPointF>
#include <string>

namespace vajra::gui {

enum class CanvasToolMode {
    SELECT,
    HAND
};

class SchematicView : public QGraphicsView {
    Q_OBJECT
public:
    explicit SchematicView(QWidget* parent = nullptr);
    explicit SchematicView(QGraphicsScene* scene, QWidget* parent = nullptr);

    void zoom_in();
    void zoom_out();
    void reset_zoom();
    void fit_to_all(double margin = 40.0);

    double get_current_zoom() const { return current_zoom_; }
    void apply_zoom_at(double factor, const QPointF& viewport_pos);

    CanvasToolMode get_tool_mode() const { return tool_mode_; }
    void set_tool_mode(CanvasToolMode mode);
    void toggle_tool_mode();

signals:
    void zoom_changed(double current_zoom);
    void mouse_moved_scene(const QPointF& scene_pos);
    void ascend_requested();
    void reset_to_top_requested();
    void drill_down_requested(const std::string& target_module);
    void tool_mode_changed(CanvasToolMode mode);
    void export_requested();

protected:
    bool viewportEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    double current_zoom_{1.0};
    CanvasToolMode tool_mode_{CanvasToolMode::SELECT};
    bool spacebar_pressed_{false};
    bool is_panning_{false};
    QPoint last_mouse_pos_;
    QPoint last_touch_pos_;

    void setup_viewport();
    void update_cursor();
    void ensure_scene_rect_covers_viewport();
    bool handle_gesture_event(QGestureEvent* event);
    void handle_pinch_gesture(QPinchGesture* gesture);
    bool handle_native_gesture_event(QNativeGestureEvent* event);
    bool handle_touch_event(QTouchEvent* event);
};

} // namespace vajra::gui

