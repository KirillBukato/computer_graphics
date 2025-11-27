#include <gtk/gtk.h>
#include <cairo.h>
#include <cmath>
#include <vector>
#include <chrono>
#include <string>
#include <iostream>
#include <algorithm>

// Структура для хранения точки
struct Point {
    int x, y;
    Point(int x = 0, int y = 0) : x(x), y(y) {}
};

// Структура для хранения точки с альфа-каналом (для Ву)
struct WuPoint {
    int x, y;
    double alpha;
    WuPoint(int x = 0, int y = 0, double alpha = 1.0) : x(x), y(y), alpha(alpha) {}
};

// Структура для хранения данных приложения
struct AppData {
    GtkWidget *drawing_area;
    GtkWidget *algorithm_combo;
    GtkWidget *time_label;
    GtkWidget *scale_spin;
    GtkWidget *grid_check;
    GtkWidget *axes_check;
    GtkWidget *coordinates_check;

    std::vector<Point> points;
    std::vector<WuPoint> wu_points;
    bool drawing;
    int start_x, start_y;
    int end_x, end_y;
    bool has_line;
    double scale;

    // Для демонстрации вычислений
    std::vector<std::string> calculations;
};

// Вспомогательная функция для дробной части
double fractional_part(double x) {
    return x - floor(x);
}

// Функции алгоритмов растеризации
void step_by_step_line(AppData *data, int x1, int y1, int x2, int y2);
void dda_line(AppData *data, int x1, int y1, int x2, int y2);
void bresenham_line(AppData *data, int x1, int y1, int x2, int y2);
void bresenham_circle(AppData *data, int xc, int yc, int r);
void wu_line(AppData *data, int x1, int y1, int x2, int y2);
void castle_pitway_line(AppData *data, int x1, int y1, int x2, int y2);

// Вспомогательные функции
void draw_grid(cairo_t *cr, int width, int height, double scale);
void draw_axes(cairo_t *cr, int width, int height);
void draw_coordinates(cairo_t *cr, int width, int height, double scale);
void put_pixel(cairo_t *cr, int x, int y, double scale, int width, int height);
void put_wu_pixel(cairo_t *cr, int x, int y, double alpha, double scale, int width, int height);

// Обработчики событий
gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data);
gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer user_data);
void on_algorithm_changed(GtkComboBox *widget, gpointer user_data);
void on_scale_changed(GtkSpinButton *widget, gpointer user_data);
void on_toggle_changed(GtkToggleButton *widget, gpointer user_data);

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Создание главного окна
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Лабораторная работа 3 - Растровые алгоритмы");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    // Инициализация данных приложения
    AppData *data = new AppData();
    data->has_line = false;
    data->drawing = false;
    data->scale = 20.0;
    data->calculations.clear();

    // Создание основного контейнера
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    // Панель управления
    GtkWidget *control_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(main_box), control_panel, FALSE, FALSE, 0);
    gtk_widget_set_size_request(control_panel, 200, -1);

    // Выбор алгоритма
    GtkWidget *algorithm_frame = gtk_frame_new("Алгоритм");
    gtk_box_pack_start(GTK_BOX(control_panel), algorithm_frame, FALSE, FALSE, 0);

    GtkWidget *algorithm_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(algorithm_combo), "Пошаговый алгоритм");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(algorithm_combo), "Алгоритм ЦДА");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(algorithm_combo), "Алгоритм Брезенхема (линия)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(algorithm_combo), "Алгоритм Брезенхема (окружность)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(algorithm_combo), "Алгоритм Ву (сглаживание)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(algorithm_combo), "Алгоритм Касла-Питвея");
    gtk_combo_box_set_active(GTK_COMBO_BOX(algorithm_combo), 0);
    gtk_container_add(GTK_CONTAINER(algorithm_frame), algorithm_combo);
    data->algorithm_combo = algorithm_combo;

    // Масштаб
    GtkWidget *scale_frame = gtk_frame_new("Масштаб");
    gtk_box_pack_start(GTK_BOX(control_panel), scale_frame, FALSE, FALSE, 0);

    GtkWidget *scale_spin = gtk_spin_button_new_with_range(5.0, 50.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(scale_spin), 20.0);
    gtk_container_add(GTK_CONTAINER(scale_frame), scale_spin);
    data->scale_spin = scale_spin;

    // Настройки отображения
    GtkWidget *display_frame = gtk_frame_new("Отображение");
    gtk_box_pack_start(GTK_BOX(control_panel), display_frame, FALSE, FALSE, 0);

    GtkWidget *display_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(display_frame), display_box);

    GtkWidget *grid_check = gtk_check_button_new_with_label("Сетка");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(grid_check), TRUE);
    gtk_box_pack_start(GTK_BOX(display_box), grid_check, FALSE, FALSE, 0);
    data->grid_check = grid_check;

    GtkWidget *axes_check = gtk_check_button_new_with_label("Оси координат");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(axes_check), TRUE);
    gtk_box_pack_start(GTK_BOX(display_box), axes_check, FALSE, FALSE, 0);
    data->axes_check = axes_check;

    GtkWidget *coordinates_check = gtk_check_button_new_with_label("Подписи координат");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(coordinates_check), TRUE);
    gtk_box_pack_start(GTK_BOX(display_box), coordinates_check, FALSE, FALSE, 0);
    data->coordinates_check = coordinates_check;

    // Время выполнения
    GtkWidget *time_frame = gtk_frame_new("Время выполнения");
    gtk_box_pack_start(GTK_BOX(control_panel), time_frame, FALSE, FALSE, 0);

    GtkWidget *time_label = gtk_label_new("Время: 0 мкс");
    gtk_container_add(GTK_CONTAINER(time_frame), time_label);
    data->time_label = time_label;

    // Область рисования
    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_box_pack_start(GTK_BOX(main_box), drawing_area, TRUE, TRUE, 0);
    data->drawing_area = drawing_area;

    // Подключение сигналов
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw), data);
    g_signal_connect(drawing_area, "button-press-event", G_CALLBACK(on_button_press), data);
    g_signal_connect(drawing_area, "button-release-event", G_CALLBACK(on_button_release), data);
    g_signal_connect(drawing_area, "motion-notify-event", G_CALLBACK(on_motion_notify), data);
    g_signal_connect(algorithm_combo, "changed", G_CALLBACK(on_algorithm_changed), data);
    g_signal_connect(scale_spin, "value-changed", G_CALLBACK(on_scale_changed), data);
    g_signal_connect(grid_check, "toggled", G_CALLBACK(on_toggle_changed), data);
    g_signal_connect(axes_check, "toggled", G_CALLBACK(on_toggle_changed), data);
    g_signal_connect(coordinates_check, "toggled", G_CALLBACK(on_toggle_changed), data);

    // Включаем события мыши
    gtk_widget_set_events(drawing_area, gtk_widget_get_events(drawing_area) |
                                        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

    gtk_widget_show_all(window);
    gtk_main();

    delete data;
    return 0;
}

// Пошаговый алгоритм
void step_by_step_line(AppData *data, int x1, int y1, int x2, int y2) {
    data->calculations.clear();
    data->calculations.push_back("=== Пошаговый алгоритм ===");
    data->calculations.push_back("Точки: (" + std::to_string(x1) + "," + std::to_string(y1) +
                                 ") -> (" + std::to_string(x2) + "," + std::to_string(y2) + ")");

    if (x1 == x2 && y1 == y2) {
        data->calculations.push_back("Точки совпадают");
        data->points.emplace_back(x1, y1);
        return;
    }

    double dx = x2 - x1;
    double dy = y2 - y1;

    data->calculations.push_back("dx = " + std::to_string(dx) + ", dy = " + std::to_string(dy));

    if (abs(dx) > abs(dy)) {
        data->calculations.push_back("|dx| > |dy| - итерация по X");
        if (x1 > x2) {
            std::swap(x1, x2);
            std::swap(y1, y2);
            dx = -dx;
            dy = -dy;
        }
        double k = dy / dx;
        data->calculations.push_back("k = dy/dx = " + std::to_string(k));

        double y = y1;
        for (int x = x1; x <= x2; x++) {
            data->points.emplace_back(x, round(y));
            data->calculations.push_back("x=" + std::to_string(x) + ", y=" + std::to_string(y) +
                                         " -> округление до " + std::to_string(round(y)));
            y += k;
        }
    } else {
        data->calculations.push_back("|dy| >= |dx| - итерация по Y");
        if (y1 > y2) {
            std::swap(x1, x2);
            std::swap(y1, y2);
            dx = -dx;
            dy = -dy;
        }
        double k = dx / dy;
        data->calculations.push_back("k = dx/dy = " + std::to_string(k));

        double x = x1;
        for (int y = y1; y <= y2; y++) {
            data->points.emplace_back(round(x), y);
            data->calculations.push_back("y=" + std::to_string(y) + ", x=" + std::to_string(x) +
                                         " -> округление до " + std::to_string(round(x)));
            x += k;
        }
    }
}

// Алгоритм ЦДА (Digital Differential Analyzer)
void dda_line(AppData *data, int x1, int y1, int x2, int y2) {
    data->calculations.clear();
    data->calculations.push_back("=== Алгоритм ЦДА ===");
    data->calculations.push_back("Точки: (" + std::to_string(x1) + "," + std::to_string(y1) +
                                 ") -> (" + std::to_string(x2) + "," + std::to_string(y2) + ")");

    int dx = x2 - x1;
    int dy = y2 - y1;
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);

    data->calculations.push_back("dx = " + std::to_string(dx) + ", dy = " + std::to_string(dy));
    data->calculations.push_back("steps = max(|dx|, |dy|) = " + std::to_string(steps));

    if (steps == 0) {
        data->calculations.push_back("Точки совпадают");
        data->points.emplace_back(x1, y1);
        return;
    }

    float xInc = dx / (float)steps;
    float yInc = dy / (float)steps;

    data->calculations.push_back("xInc = dx/steps = " + std::to_string(xInc));
    data->calculations.push_back("yInc = dy/steps = " + std::to_string(yInc));

    float x = x1;
    float y = y1;

    for (int i = 0; i <= steps; i++) {
        data->points.emplace_back(round(x), round(y));
        data->calculations.push_back("Шаг " + std::to_string(i) + ": x=" + std::to_string(x) +
                                     ", y=" + std::to_string(y) + " -> (" +
                                     std::to_string(round(x)) + "," + std::to_string(round(y)) + ")");
        x += xInc;
        y += yInc;
    }
}

// Алгоритм Брезенхема для линии
void bresenham_line(AppData *data, int x1, int y1, int x2, int y2) {
    data->calculations.clear();
    data->calculations.push_back("=== Алгоритм Брезенхема (линия) ===");
    data->calculations.push_back("Точки: (" + std::to_string(x1) + "," + std::to_string(y1) +
                                 ") -> (" + std::to_string(x2) + "," + std::to_string(y2) + ")");

    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    data->calculations.push_back("dx = " + std::to_string(dx) + ", dy = " + std::to_string(dy));
    data->calculations.push_back("sx = " + std::to_string(sx) + ", sy = " + std::to_string(sy));
    data->calculations.push_back("Начальная ошибка err = dx - dy = " + std::to_string(err));

    int x = x1;
    int y = y1;
    int step = 0;

    data->points.emplace_back(x, y);
    data->calculations.push_back("Шаг " + std::to_string(step++) + ": (" +
                                 std::to_string(x) + "," + std::to_string(y) +
                                 "), err = " + std::to_string(err));

    while (x != x2 || y != y2) {
        int e2 = 2 * err;
        data->calculations.push_back("  e2 = 2 * err = " + std::to_string(e2));

        if (e2 > -dy) {
            err -= dy;
            x += sx;
            data->calculations.push_back("  e2 > -dy -> err -= dy = " + std::to_string(err) +
                                         ", x += sx = " + std::to_string(x));
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
            data->calculations.push_back("  e2 < dx -> err += dx = " + std::to_string(err) +
                                         ", y += sy = " + std::to_string(y));
        }

        data->points.emplace_back(x, y);
        data->calculations.push_back("Шаг " + std::to_string(step++) + ": (" +
                                     std::to_string(x) + "," + std::to_string(y) +
                                     "), err = " + std::to_string(err));
    }
}

// Алгоритм Брезенхема для окружности
void bresenham_circle(AppData *data, int xc, int yc, int r) {
    data->calculations.clear();
    data->calculations.push_back("=== Алгоритм Брезенхема (окружность) ===");
    data->calculations.push_back("Центр: (" + std::to_string(xc) + "," + std::to_string(yc) +
                                 "), радиус: " + std::to_string(r));

    if (r <= 0) {
        data->calculations.push_back("Радиус должен быть положительным");
        data->points.emplace_back(xc, yc);
        return;
    }

    int x = 0;
    int y = r;
    int d = 3 - 2 * r;

    data->calculations.push_back("Начальные значения: x=0, y=" + std::to_string(r) +
                                 ", d=3-2*r=" + std::to_string(d));

    auto plot_circle_points = [&](int x, int y) {
        data->points.emplace_back(xc + x, yc + y);
        data->points.emplace_back(xc - x, yc + y);
        data->points.emplace_back(xc + x, yc - y);
        data->points.emplace_back(xc - x, yc - y);
        data->points.emplace_back(xc + y, yc + x);
        data->points.emplace_back(xc - y, yc + x);
        data->points.emplace_back(xc + y, yc - x);
        data->points.emplace_back(xc - y, yc - x);
    };

    plot_circle_points(x, y);
    int step = 0;
    data->calculations.push_back("Шаг " + std::to_string(step++) + ": x=" +
                                 std::to_string(x) + ", y=" + std::to_string(y) +
                                 ", d=" + std::to_string(d));

    while (y >= x) {
        x++;

        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
            data->calculations.push_back("  d > 0 -> y--, d = d + 4*(x-y) + 10 = " + std::to_string(d));
        } else {
            d = d + 4 * x + 6;
            data->calculations.push_back("  d <= 0 -> d = d + 4*x + 6 = " + std::to_string(d));
        }

        plot_circle_points(x, y);
        data->calculations.push_back("Шаг " + std::to_string(step++) + ": x=" +
                                     std::to_string(x) + ", y=" + std::to_string(y) +
                                     ", d=" + std::to_string(d));
    }
}

// Алгоритм Ву (сглаживание)
void wu_line(AppData *data, int x1, int y1, int x2, int y2) {
    data->calculations.clear();
    data->wu_points.clear();
    data->calculations.push_back("=== Алгоритм Ву (сглаживание) ===");
    data->calculations.push_back("Точки: (" + std::to_string(x1) + "," + std::to_string(y1) +
                                 ") -> (" + std::to_string(x2) + "," + std::to_string(y2) + ")");

    // Функция для рисования точки с интенсивностью
    auto plot = [&](int x, int y, double alpha) {
        data->wu_points.emplace_back(x, y, alpha);
    };

    bool steep = abs(y2 - y1) > abs(x2 - x1);

    if (steep) {
        std::swap(x1, y1);
        std::swap(x2, y2);
        data->calculations.push_back("Крутой отрезок (|dy| > |dx|), меняем x и y местами");
    }

    if (x1 > x2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
        data->calculations.push_back("Меняем точки местами для рисования слева направо");
    }

    double dx = x2 - x1;
    double dy = y2 - y1;
    double gradient = (dx == 0) ? 1.0 : dy / dx;

    data->calculations.push_back("dx = " + std::to_string(dx) + ", dy = " + std::to_string(dy));
    data->calculations.push_back("gradient = dy/dx = " + std::to_string(gradient));

    // Первая конечная точка
    double xend = round(x1);
    double yend = y1 + gradient * (xend - x1);
    double xgap = 1.0 - fractional_part(x1 + 0.5);
    int xpxl1 = xend;
    int ypxl1 = floor(yend);

    if (steep) {
        plot(ypxl1, xpxl1, (1.0 - fractional_part(yend)) * xgap);
        plot(ypxl1 + 1, xpxl1, fractional_part(yend) * xgap);
    } else {
        plot(xpxl1, ypxl1, (1.0 - fractional_part(yend)) * xgap);
        plot(xpxl1, ypxl1 + 1, fractional_part(yend) * xgap);
    }

    data->calculations.push_back("Первая точка: x=" + std::to_string(xpxl1) + ", y=" + std::to_string(ypxl1) +
                                 ", alpha1=" + std::to_string((1.0 - fractional_part(yend)) * xgap) +
                                 ", alpha2=" + std::to_string(fractional_part(yend) * xgap));

    double intery = yend + gradient;

    // Вторая конечная точка
    xend = round(x2);
    yend = y2 + gradient * (xend - x2);
    xgap = fractional_part(x2 + 0.5);
    int xpxl2 = xend;
    int ypxl2 = floor(yend);

    if (steep) {
        plot(ypxl2, xpxl2, (1.0 - fractional_part(yend)) * xgap);
        plot(ypxl2 + 1, xpxl2, fractional_part(yend) * xgap);
    } else {
        plot(xpxl2, ypxl2, (1.0 - fractional_part(yend)) * xgap);
        plot(xpxl2, ypxl2 + 1, fractional_part(yend) * xgap);
    }

    data->calculations.push_back("Последняя точка: x=" + std::to_string(xpxl2) + ", y=" + std::to_string(ypxl2) +
                                 ", alpha1=" + std::to_string((1.0 - fractional_part(yend)) * xgap) +
                                 ", alpha2=" + std::to_string(fractional_part(yend) * xgap));

    // Основной цикл
    data->calculations.push_back("Основной цикл:");
    for (int x = xpxl1 + 1; x < xpxl2; x++) {
        if (steep) {
            plot(floor(intery), x, 1.0 - fractional_part(intery));
            plot(floor(intery) + 1, x, fractional_part(intery));
        } else {
            plot(x, floor(intery), 1.0 - fractional_part(intery));
            plot(x, floor(intery) + 1, fractional_part(intery));
        }

        data->calculations.push_back("x=" + std::to_string(x) + ", y=" + std::to_string(floor(intery)) +
                                     ", alpha1=" + std::to_string(1.0 - fractional_part(intery)) +
                                     ", alpha2=" + std::to_string(fractional_part(intery)));

        intery += gradient;
    }
}

// Алгоритм Касла-Питвея (Castle-Pitway)
void castle_pitway_line(AppData *data, int x1, int y1, int x2, int y2) {
    data->calculations.clear();
    data->calculations.push_back("=== Алгоритм Касла-Питвея ===");
    data->calculations.push_back("Точки: (" + std::to_string(x1) + "," + std::to_string(y1) +
                                 ") -> (" + std::to_string(x2) + "," + std::to_string(y2) + ")");

    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);

    data->calculations.push_back("dx = " + std::to_string(dx) + ", dy = " + std::to_string(dy));

    if (dx == 0 && dy == 0) {
        data->points.emplace_back(x1, y1);
        data->calculations.push_back("Точки совпадают");
        return;
    }

    // Определяем октант
    int octant = 0;
    if (dx >= dy) {
        if (x2 >= x1) {
            octant = (y2 >= y1) ? 0 : 3;
        } else {
            octant = (y2 >= y1) ? 4 : 7;
        }
    } else {
        if (y2 >= y1) {
            octant = (x2 >= x1) ? 1 : 2;
        } else {
            octant = (x2 >= x1) ? 6 : 5;
        }
    }

    data->calculations.push_back("Октант: " + std::to_string(octant));

    // Приводим к первому октанту
    int x1_prim = x1, y1_prim = y1, x2_prim = x2, y2_prim = y2;

    switch (octant) {
        case 0: break; // Уже в первом октанте
        case 1: std::swap(x1_prim, y1_prim); std::swap(x2_prim, y2_prim); break;
        case 2: x1_prim = -x1; y1_prim = y1; x2_prim = -x2; y2_prim = y2; std::swap(x1_prim, y1_prim); std::swap(x2_prim, y2_prim); break;
        case 3: y1_prim = -y1; y2_prim = -y2; break;
        case 4: x1_prim = -x1; x2_prim = -x2; break;
        case 5: x1_prim = -x1; y1_prim = -y1; x2_prim = -x2; y2_prim = -y2; std::swap(x1_prim, y1_prim); std::swap(x2_prim, y2_prim); break;
        case 6: x1_prim = x1; y1_prim = -y1; x2_prim = x2; y2_prim = -y2; std::swap(x1_prim, y1_prim); std::swap(x2_prim, y2_prim); break;
        case 7: x1_prim = -x1; y1_prim = -y1; x2_prim = -x2; y2_prim = -y2; break;
    }

    data->calculations.push_back("После приведения к 1-му октанту: (" +
                                 std::to_string(x1_prim) + "," + std::to_string(y1_prim) +
                                 ") -> (" + std::to_string(x2_prim) + "," + std::to_string(y2_prim) + ")");

    // Алгоритм Брезенхема для первого октанта
    int dx_prim = x2_prim - x1_prim;
    int dy_prim = y2_prim - y1_prim;
    int error = 2 * dy_prim - dx_prim;

    data->calculations.push_back("dx' = " + std::to_string(dx_prim) + ", dy' = " + std::to_string(dy_prim));
    data->calculations.push_back("Начальная ошибка = 2*dy' - dx' = " + std::to_string(error));

    int x = x1_prim;
    int y = y1_prim;
    int step = 0;

    // Функция для обратного преобразования координат
    auto transform_back = [&](int x_prim, int y_prim) -> Point {
        int x_ret = x_prim, y_ret = y_prim;
        switch (octant) {
            case 0: break;
            case 1: std::swap(x_ret, y_ret); break;
            case 2: std::swap(x_ret, y_ret); x_ret = -x_ret; break;
            case 3: y_ret = -y_ret; break;
            case 4: x_ret = -x_ret; break;
            case 5: std::swap(x_ret, y_ret); x_ret = -x_ret; y_ret = -y_ret; break;
            case 6: std::swap(x_ret, y_ret); y_ret = -y_ret; break;
            case 7: x_ret = -x_ret; y_ret = -y_ret; break;
        }
        return Point(x_ret, y_ret);
    };

    data->points.push_back(transform_back(x, y));
    data->calculations.push_back("Шаг " + std::to_string(step++) + ": (" +
                                 std::to_string(data->points.back().x) + "," +
                                 std::to_string(data->points.back().y) + "), ошибка=" +
                                 std::to_string(error));

    while (x < x2_prim) {
        x++;
        if (error > 0) {
            y++;
            error += 2 * (dy_prim - dx_prim);
            data->calculations.push_back("Ошибка > 0 → y++, ошибка += 2*(dy'-dx') = " + std::to_string(error));
        } else {
            error += 2 * dy_prim;
            data->calculations.push_back("Ошибка <= 0 → ошибка += 2*dy' = " + std::to_string(error));
        }

        data->points.push_back(transform_back(x, y));
        data->calculations.push_back("Шаг " + std::to_string(step++) + ": (" +
                                     std::to_string(data->points.back().x) + "," +
                                     std::to_string(data->points.back().y) + "), ошибка=" +
                                     std::to_string(error));
    }
}

// Отрисовка сетки
void draw_grid(cairo_t *cr, int width, int height, double scale) {
    cairo_save(cr);

    // Центр координат
    int center_x = width / 2;
    int center_y = height / 2;

    cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.5);
    cairo_set_line_width(cr, 0.5);

    // Вертикальные линии
    for (int x = center_x; x < width; x += scale) {
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, height);
    }
    for (int x = center_x; x > 0; x -= scale) {
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, height);
    }

    // Горизонтальные линии
    for (int y = center_y; y < height; y += scale) {
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
    }
    for (int y = center_y; y > 0; y -= scale) {
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
    }

    cairo_stroke(cr);
    cairo_restore(cr);
}

// Отрисовка осей координат
void draw_axes(cairo_t *cr, int width, int height) {
    cairo_save(cr);

    int center_x = width / 2;
    int center_y = height / 2;

    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 1.5);

    // Ось X
    cairo_move_to(cr, 0, center_y);
    cairo_line_to(cr, width, center_y);

    // Ось Y
    cairo_move_to(cr, center_x, 0);
    cairo_line_to(cr, center_x, height);

    // Стрелки
    cairo_move_to(cr, width - 10, center_y - 5);
    cairo_line_to(cr, width, center_y);
    cairo_line_to(cr, width - 10, center_y + 5);

    cairo_move_to(cr, center_x - 5, 10);
    cairo_line_to(cr, center_x, 0);
    cairo_line_to(cr, center_x + 5, 10);

    cairo_stroke(cr);
    cairo_restore(cr);
}

// Отрисовка подписей координат
void draw_coordinates(cairo_t *cr, int width, int height, double scale) {
    cairo_save(cr);

    int center_x = width / 2;
    int center_y = height / 2;

    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);

    // Подписи по оси X
    for (int x = center_x + scale; x < width; x += scale) {
        int value = (x - center_x) / scale;
        std::string label = std::to_string(value);
        cairo_move_to(cr, x - 5, center_y + 15);
        cairo_show_text(cr, label.c_str());
    }
    for (int x = center_x - scale; x > 0; x -= scale) {
        int value = (x - center_x) / scale;
        std::string label = std::to_string(value);
        cairo_move_to(cr, x - 5, center_y + 15);
        cairo_show_text(cr, label.c_str());
    }

    // Подписи по оси Y
    for (int y = center_y + scale; y < height; y += scale) {
        int value = (center_y - y) / scale;
        std::string label = std::to_string(value);
        cairo_move_to(cr, center_x + 10, y + 5);
        cairo_show_text(cr, label.c_str());
    }
    for (int y = center_y - scale; y > 0; y -= scale) {
        int value = (center_y - y) / scale;
        std::string label = std::to_string(value);
        cairo_move_to(cr, center_x + 10, y + 5);
        cairo_show_text(cr, label.c_str());
    }

    // Подписи осей
    cairo_move_to(cr, width - 20, center_y - 10);
    cairo_show_text(cr, "X");
    cairo_move_to(cr, center_x + 10, 20);
    cairo_show_text(cr, "Y");
    cairo_move_to(cr, center_x - 10, center_y + 20);
    cairo_show_text(cr, "O");

    cairo_restore(cr);
}

// Отрисовка пикселя
void put_pixel(cairo_t *cr, int x, int y, double scale, int width, int height) {
    int center_x = width / 2;
    int center_y = height / 2;

    double screen_x = center_x + x * scale;
    double screen_y = center_y - y * scale;

    // Проверка границ
    if (screen_x < 0 || screen_x >= width || screen_y < 0 || screen_y >= height) {
        return;
    }

    cairo_rectangle(cr, screen_x, screen_y, scale, scale);
    cairo_fill(cr);
}

// Отрисовка пикселя для алгоритма Ву
void put_wu_pixel(cairo_t *cr, int x, int y, double alpha, double scale, int width, int height) {
    int center_x = width / 2;
    int center_y = height / 2;

    double screen_x = center_x + x * scale;
    double screen_y = center_y - y * scale;

    // Проверка границ
    if (screen_x < 0 || screen_x >= width || screen_y < 0 || screen_y >= height) {
        return;
    }

    // Устанавливаем цвет с альфа-каналом
    cairo_set_source_rgba(cr, 1, 0, 0, alpha);
    cairo_rectangle(cr, screen_x, screen_y, scale, scale);
    cairo_fill(cr);
}

// Обработчик отрисовки
gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    AppData *data = static_cast<AppData*>(user_data);
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);

    // Очистка фона
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    // Отрисовка сетки
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->grid_check))) {
        draw_grid(cr, width, height, data->scale);
    }

    // Отрисовка осей
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->axes_check))) {
        draw_axes(cr, width, height);
    }

    // Отрисовка подписей координат
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->coordinates_check))) {
        draw_coordinates(cr, width, height, data->scale);
    }

    // Отрисовка точек (для обычных алгоритмов)
    if (!data->points.empty()) {
        cairo_set_source_rgb(cr, 1, 0, 0);

        for (const auto& point : data->points) {
            put_pixel(cr, point.x, point.y, data->scale, width, height);
        }
    }

    // Отрисовка точек для алгоритма Ву
    if (!data->wu_points.empty()) {
        for (const auto& point : data->wu_points) {
            put_wu_pixel(cr, point.x, point.y, point.alpha, data->scale, width, height);
        }
    }

    return FALSE;
}

// Обработчики событий
gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    AppData *data = static_cast<AppData*>(user_data);

    if (event->button == GDK_BUTTON_PRIMARY) {
        data->drawing = true;

        int width = gtk_widget_get_allocated_width(widget);
        int height = gtk_widget_get_allocated_height(widget);
        int center_x = width / 2;
        int center_y = height / 2;

        data->start_x = round((event->x - center_x) / data->scale);
        data->start_y = round((center_y - event->y) / data->scale);

        data->points.clear();
        data->wu_points.clear();
        std::cout << "Начальная точка: (" << data->start_x << ", " << data->start_y << ")" << std::endl;
    }

    return TRUE;
}

// Функция для перерисовки линии текущим алгоритмом
void redraw_line(AppData *data) {
    if (!data->has_line) return;

    // Измерение времени выполнения
    auto start_time = std::chrono::high_resolution_clock::now();

    int algorithm = gtk_combo_box_get_active(GTK_COMBO_BOX(data->algorithm_combo));

    data->points.clear();
    data->wu_points.clear();

    switch (algorithm) {
        case 0: // Пошаговый
            step_by_step_line(data, data->start_x, data->start_y, data->end_x, data->end_y);
            break;
        case 1: // ЦДА
            dda_line(data, data->start_x, data->start_y, data->end_x, data->end_y);
            break;
        case 2: // Брезенхем (линия)
            bresenham_line(data, data->start_x, data->start_y, data->end_x, data->end_y);
            break;
        case 3: { // Брезенхем (окружность)
            int radius = static_cast<int>(sqrt(pow(data->end_x - data->start_x, 2) +
                                               pow(data->end_y - data->start_y, 2)));
            bresenham_circle(data, data->start_x, data->start_y, radius);
            break;
        }
        case 4: // Ву
            wu_line(data, data->start_x, data->start_y, data->end_x, data->end_y);
            break;
        case 5: // Касла-Питвея
            castle_pitway_line(data, data->start_x, data->start_y, data->end_x, data->end_y);
            break;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    // Обновление времени выполнения
    std::string time_text = "Время: " + std::to_string(duration.count()) + " мкс";
    gtk_label_set_text(GTK_LABEL(data->time_label), time_text.c_str());

    gtk_widget_queue_draw(data->drawing_area);
}

gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    AppData *data = static_cast<AppData*>(user_data);

    if (event->button == GDK_BUTTON_PRIMARY && data->drawing) {
        data->drawing = false;

        int width = gtk_widget_get_allocated_width(widget);
        int height = gtk_widget_get_allocated_height(widget);
        int center_x = width / 2;
        int center_y = height / 2;

        data->end_x = round((event->x - center_x) / data->scale);
        data->end_y = round((center_y - event->y) / data->scale);
        data->has_line = true;

        std::cout << "Конечная точка: (" << data->end_x << ", " << data->end_y << ")" << std::endl;

        // Перерисовываем линию
        redraw_line(data);

        // Вывод вычислений в консоль
        std::cout << "\n=== Вычисления для алгоритма ===" << std::endl;
        for (const auto& calc : data->calculations) {
            std::cout << calc << std::endl;
        }
        std::cout << "================================" << std::endl;
        std::cout << "Количество точек: " << (data->points.size() + data->wu_points.size()) << std::endl;
    }

    return TRUE;
}

gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    // Можно добавить предварительный просмотр
    return TRUE;
}

void on_algorithm_changed(GtkComboBox *widget, gpointer user_data) {
    AppData *data = static_cast<AppData*>(user_data);

    // Если линия уже была нарисована, перерисовываем её новым алгоритмом
    if (data->has_line) {
        redraw_line(data);
    } else {
        data->points.clear();
        data->wu_points.clear();
        gtk_widget_queue_draw(data->drawing_area);
    }
}

void on_scale_changed(GtkSpinButton *widget, gpointer user_data) {
    AppData *data = static_cast<AppData*>(user_data);
    data->scale = gtk_spin_button_get_value(widget);
    gtk_widget_queue_draw(data->drawing_area);
}

void on_toggle_changed(GtkToggleButton *widget, gpointer user_data) {
    AppData *data = static_cast<AppData*>(user_data);
    gtk_widget_queue_draw(data->drawing_area);
}