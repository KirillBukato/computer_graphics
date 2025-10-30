#include <gtk-3.0/gtk/gtk.h>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>

class Color {
   public:
    virtual ~Color() = default;
};

class RGB : public Color {
   public:
    double red;
    double green;
    double blue;

    RGB() : red(0), green(0), blue(0) {}
    RGB(double r, double g, double b) : red(r), green(g), blue(b) {}
};

class CMYK : public Color {
   public:
    double cyan;
    double magenta;
    double yellow;
    double black;

    CMYK() : cyan(0), magenta(0), yellow(0), black(0) {}
    CMYK(double c, double m, double y, double k) : cyan(c), magenta(m), yellow(y), black(k) {}
};

class HSV : public Color {
   public:
    double hue;
    double saturation;
    double value;

    HSV() : hue(0), saturation(0), value(0) {}
    HSV(double h, double s, double v) : hue(h), saturation(s), value(v) {}
};

// Глобальные переменные для виджетов
GtkWidget *rgb_red_scale, *rgb_green_scale, *rgb_blue_scale;
GtkWidget *cmyk_cyan_scale, *cmyk_magenta_scale, *cmyk_yellow_scale, *cmyk_black_scale;
GtkWidget *hsv_hue_scale, *hsv_saturation_scale, *hsv_value_scale;
GtkWidget *rgb_red_entry, *rgb_green_entry, *rgb_blue_entry;
GtkWidget *cmyk_cyan_entry, *cmyk_magenta_entry, *cmyk_yellow_entry, *cmyk_black_entry;
GtkWidget *hsv_hue_entry, *hsv_saturation_entry, *hsv_value_entry;
GtkWidget *color_preview;
GtkWidget *color_picker_button;

// Флаги для предотвращения рекурсивных обновлений
bool updating = false;

// Функции преобразования между цветовыми моделями
RGB CMYKtoRGB(const CMYK& cmyk) {
    double r = (1 - cmyk.cyan) * (1 - cmyk.black);
    double g = (1 - cmyk.magenta) * (1 - cmyk.black);
    double b = (1 - cmyk.yellow) * (1 - cmyk.black);
    return RGB(r, g, b);
}

CMYK RGBtoCMYK(const RGB& rgb) {
    double k = 1 - std::max({rgb.red, rgb.green, rgb.blue});
    if (k == 1) {
        return CMYK(0, 0, 0, 1);
    }
    double c = (1 - rgb.red - k) / (1 - k);
    double m = (1 - rgb.green - k) / (1 - k);
    double y = (1 - rgb.blue - k) / (1 - k);
    return CMYK(c, m, y, k);
}

HSV RGBtoHSV(const RGB& rgb) {
    double r = rgb.red;
    double g = rgb.green;
    double b = rgb.blue;

    double max = std::max({r, g, b});
    double min = std::min({r, g, b});
    double delta = max - min;

    double h = 0, s = 0, v = max;

    if (delta > 0) {
        s = delta / max;

        if (max == r) {
            h = (g - b) / delta;
            if (g < b) h += 6;
        } else if (max == g) {
            h = 2 + (b - r) / delta;
        } else {
            h = 4 + (r - g) / delta;
        }

        h *= 60;
        if (h < 0) h += 360;
    }

    return HSV(h, s, v);
}

RGB HSVtoRGB(const HSV& hsv) {
    double h = hsv.hue;
    double s = hsv.saturation;
    double v = hsv.value;

    if (s == 0) {
        return RGB(v, v, v);
    }

    h /= 60;
    int i = static_cast<int>(h);
    double f = h - i;
    double p = v * (1 - s);
    double q = v * (1 - s * f);
    double t = v * (1 - s * (1 - f));

    switch (i) {
        case 0: return RGB(v, t, p);
        case 1: return RGB(q, v, p);
        case 2: return RGB(p, v, t);
        case 3: return RGB(p, q, v);
        case 4: return RGB(t, p, v);
        default: return RGB(v, p, q);
    }
}

// Функции обновления интерфейса
void update_color_preview(const RGB& rgb) {
    GdkRGBA color;
    color.red = rgb.red;
    color.green = rgb.green;
    color.blue = rgb.blue;
    color.alpha = 1.0;

    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_picker_button), &color);

    std::string css = ".color-preview { background: rgb(" +
                      std::to_string(static_cast<int>(rgb.red * 255)) + "," +
                      std::to_string(static_cast<int>(rgb.green * 255)) + "," +
                      std::to_string(static_cast<int>(rgb.blue * 255)) + "); }";

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css.c_str(), -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(color_preview),
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);
}

void update_rgb_ui(const RGB& rgb) {
    if (updating) return;
    updating = true;

    gtk_range_set_value(GTK_RANGE(rgb_red_scale), rgb.red * 100);
    gtk_range_set_value(GTK_RANGE(rgb_green_scale), rgb.green * 100);
    gtk_range_set_value(GTK_RANGE(rgb_blue_scale), rgb.blue * 100);

    gtk_entry_set_text(GTK_ENTRY(rgb_red_entry), std::to_string(static_cast<int>(rgb.red * 255)).c_str());
    gtk_entry_set_text(GTK_ENTRY(rgb_green_entry), std::to_string(static_cast<int>(rgb.green * 255)).c_str());
    gtk_entry_set_text(GTK_ENTRY(rgb_blue_entry), std::to_string(static_cast<int>(rgb.blue * 255)).c_str());

    updating = false;
}

void update_cmyk_ui(const CMYK& cmyk) {
    if (updating) return;
    updating = true;

    gtk_range_set_value(GTK_RANGE(cmyk_cyan_scale), cmyk.cyan * 100);
    gtk_range_set_value(GTK_RANGE(cmyk_magenta_scale), cmyk.magenta * 100);
    gtk_range_set_value(GTK_RANGE(cmyk_yellow_scale), cmyk.yellow * 100);
    gtk_range_set_value(GTK_RANGE(cmyk_black_scale), cmyk.black * 100);

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1);

    stream << cmyk.cyan * 100;
    gtk_entry_set_text(GTK_ENTRY(cmyk_cyan_entry), stream.str().c_str());
    stream.str("");

    stream << cmyk.magenta * 100;
    gtk_entry_set_text(GTK_ENTRY(cmyk_magenta_entry), stream.str().c_str());
    stream.str("");

    stream << cmyk.yellow * 100;
    gtk_entry_set_text(GTK_ENTRY(cmyk_yellow_entry), stream.str().c_str());
    stream.str("");

    stream << cmyk.black * 100;
    gtk_entry_set_text(GTK_ENTRY(cmyk_black_entry), stream.str().c_str());

    updating = false;
}

void update_hsv_ui(const HSV& hsv) {
    if (updating) return;
    updating = true;

    gtk_range_set_value(GTK_RANGE(hsv_hue_scale), hsv.hue);
    gtk_range_set_value(GTK_RANGE(hsv_saturation_scale), hsv.saturation * 100);
    gtk_range_set_value(GTK_RANGE(hsv_value_scale), hsv.value * 100);

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1);

    stream << hsv.hue;
    gtk_entry_set_text(GTK_ENTRY(hsv_hue_entry), stream.str().c_str());
    stream.str("");

    stream << hsv.saturation * 100;
    gtk_entry_set_text(GTK_ENTRY(hsv_saturation_entry), stream.str().c_str());
    stream.str("");

    stream << hsv.value * 100;
    gtk_entry_set_text(GTK_ENTRY(hsv_value_entry), stream.str().c_str());

    updating = false;
}

void update_all_from_rgb(const RGB& rgb) {
    CMYK cmyk = RGBtoCMYK(rgb);
    HSV hsv = RGBtoHSV(rgb);

    update_rgb_ui(rgb);
    update_cmyk_ui(cmyk);
    update_hsv_ui(hsv);
    update_color_preview(rgb);
}

// Обработчики сигналов
void on_rgb_scale_changed(GtkRange* range, gpointer user_data) {
    if (updating) return;

    RGB rgb;
    rgb.red = gtk_range_get_value(GTK_RANGE(rgb_red_scale)) / 100.0;
    rgb.green = gtk_range_get_value(GTK_RANGE(rgb_green_scale)) / 100.0;
    rgb.blue = gtk_range_get_value(GTK_RANGE(rgb_blue_scale)) / 100.0;

    update_all_from_rgb(rgb);
}

void on_cmyk_scale_changed(GtkRange* range, gpointer user_data) {
    if (updating) return;

    CMYK cmyk;
    cmyk.cyan = gtk_range_get_value(GTK_RANGE(cmyk_cyan_scale)) / 100.0;
    cmyk.magenta = gtk_range_get_value(GTK_RANGE(cmyk_magenta_scale)) / 100.0;
    cmyk.yellow = gtk_range_get_value(GTK_RANGE(cmyk_yellow_scale)) / 100.0;
    cmyk.black = gtk_range_get_value(GTK_RANGE(cmyk_black_scale)) / 100.0;

    RGB rgb = CMYKtoRGB(cmyk);
    update_all_from_rgb(rgb);
}

void on_hsv_scale_changed(GtkRange* range, gpointer user_data) {
    if (updating) return;

    HSV hsv;
    hsv.hue = gtk_range_get_value(GTK_RANGE(hsv_hue_scale));
    hsv.saturation = gtk_range_get_value(GTK_RANGE(hsv_saturation_scale)) / 100.0;
    hsv.value = gtk_range_get_value(GTK_RANGE(hsv_value_scale)) / 100.0;

    RGB rgb = HSVtoRGB(hsv);
    update_all_from_rgb(rgb);
}

void on_rgb_entry_activated(GtkEntry* entry, gpointer user_data) {
    if (updating) return;

    RGB rgb;
    rgb.red = std::stod(gtk_entry_get_text(GTK_ENTRY(rgb_red_entry))) / 255.0;
    rgb.green = std::stod(gtk_entry_get_text(GTK_ENTRY(rgb_green_entry))) / 255.0;
    rgb.blue = std::stod(gtk_entry_get_text(GTK_ENTRY(rgb_blue_entry))) / 255.0;

    // Ограничение значений
    rgb.red = std::clamp(rgb.red, 0.0, 1.0);
    rgb.green = std::clamp(rgb.green, 0.0, 1.0);
    rgb.blue = std::clamp(rgb.blue, 0.0, 1.0);

    update_all_from_rgb(rgb);
}

void on_cmyk_entry_activated(GtkEntry* entry, gpointer user_data) {
    if (updating) return;

    CMYK cmyk;
    cmyk.cyan = std::stod(gtk_entry_get_text(GTK_ENTRY(cmyk_cyan_entry))) / 100.0;
    cmyk.magenta = std::stod(gtk_entry_get_text(GTK_ENTRY(cmyk_magenta_entry))) / 100.0;
    cmyk.yellow = std::stod(gtk_entry_get_text(GTK_ENTRY(cmyk_yellow_entry))) / 100.0;
    cmyk.black = std::stod(gtk_entry_get_text(GTK_ENTRY(cmyk_black_entry))) / 100.0;

    // Ограничение значений
    cmyk.cyan = std::clamp(cmyk.cyan, 0.0, 1.0);
    cmyk.magenta = std::clamp(cmyk.magenta, 0.0, 1.0);
    cmyk.yellow = std::clamp(cmyk.yellow, 0.0, 1.0);
    cmyk.black = std::clamp(cmyk.black, 0.0, 1.0);

    RGB rgb = CMYKtoRGB(cmyk);
    update_all_from_rgb(rgb);
}

void on_hsv_entry_activated(GtkEntry* entry, gpointer user_data) {
    if (updating) return;

    HSV hsv;
    hsv.hue = std::stod(gtk_entry_get_text(GTK_ENTRY(hsv_hue_entry)));
    hsv.saturation = std::stod(gtk_entry_get_text(GTK_ENTRY(hsv_saturation_entry))) / 100.0;
    hsv.value = std::stod(gtk_entry_get_text(GTK_ENTRY(hsv_value_entry))) / 100.0;

    // Ограничение значений
    hsv.hue = std::clamp(hsv.hue, 0.0, 360.0);
    hsv.saturation = std::clamp(hsv.saturation, 0.0, 1.0);
    hsv.value = std::clamp(hsv.value, 0.0, 1.0);

    RGB rgb = HSVtoRGB(hsv);
    update_all_from_rgb(rgb);
}

void on_color_picker_changed(GtkColorButton* button, gpointer user_data) {
    if (updating) return;

    GdkRGBA color;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);

    RGB rgb;
    rgb.red = color.red;
    rgb.green = color.green;
    rgb.blue = color.blue;

    update_all_from_rgb(rgb);
}

// Создание виджетов
GtkWidget* create_scale_with_entry(const char* label, double min, double max, double step,
                                   GtkWidget** scale, GtkWidget** entry,
                                   GCallback scale_handler, GCallback entry_handler) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *label_widget = gtk_label_new(label);
    *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, step);
    *entry = gtk_entry_new();

    gtk_scale_set_digits(GTK_SCALE(*scale), 1);
    gtk_range_set_value(GTK_RANGE(*scale), min);
    gtk_entry_set_text(GTK_ENTRY(*entry), "0");

    g_signal_connect(*scale, "value-changed", scale_handler, NULL);
    g_signal_connect(*entry, "activate", entry_handler, NULL);

    gtk_box_pack_start(GTK_BOX(box), label_widget, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), *scale, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), *entry, FALSE, FALSE, 0);

    return box;
}

GtkWidget* create_rgb_section() {
    GtkWidget *frame = gtk_frame_new("RGB Color Model");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    GtkWidget *red_box = create_scale_with_entry("Red", 0, 100, 1, &rgb_red_scale, &rgb_red_entry,
                                                 G_CALLBACK(on_rgb_scale_changed), G_CALLBACK(on_rgb_entry_activated));
    GtkWidget *green_box = create_scale_with_entry("Green", 0, 100, 1, &rgb_green_scale, &rgb_green_entry,
                                                   G_CALLBACK(on_rgb_scale_changed), G_CALLBACK(on_rgb_entry_activated));
    GtkWidget *blue_box = create_scale_with_entry("Blue", 0, 100, 1, &rgb_blue_scale, &rgb_blue_entry,
                                                  G_CALLBACK(on_rgb_scale_changed), G_CALLBACK(on_rgb_entry_activated));

    gtk_box_pack_start(GTK_BOX(box), red_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), green_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), blue_box, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(frame), box);
    return frame;
}

GtkWidget* create_cmyk_section() {
    GtkWidget *frame = gtk_frame_new("CMYK Color Model");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    GtkWidget *cyan_box = create_scale_with_entry("Cyan", 0, 100, 1, &cmyk_cyan_scale, &cmyk_cyan_entry,
                                                  G_CALLBACK(on_cmyk_scale_changed), G_CALLBACK(on_cmyk_entry_activated));
    GtkWidget *magenta_box = create_scale_with_entry("Magenta", 0, 100, 1, &cmyk_magenta_scale, &cmyk_magenta_entry,
                                                     G_CALLBACK(on_cmyk_scale_changed), G_CALLBACK(on_cmyk_entry_activated));
    GtkWidget *yellow_box = create_scale_with_entry("Yellow", 0, 100, 1, &cmyk_yellow_scale, &cmyk_yellow_entry,
                                                    G_CALLBACK(on_cmyk_scale_changed), G_CALLBACK(on_cmyk_entry_activated));
    GtkWidget *black_box = create_scale_with_entry("Black", 0, 100, 1, &cmyk_black_scale, &cmyk_black_entry,
                                                   G_CALLBACK(on_cmyk_scale_changed), G_CALLBACK(on_cmyk_entry_activated));

    gtk_box_pack_start(GTK_BOX(box), cyan_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), magenta_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), yellow_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), black_box, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(frame), box);
    return frame;
}

GtkWidget* create_hsv_section() {
    GtkWidget *frame = gtk_frame_new("HSV Color Model");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    GtkWidget *hue_box = create_scale_with_entry("Hue", 0, 360, 1, &hsv_hue_scale, &hsv_hue_entry,
                                                 G_CALLBACK(on_hsv_scale_changed), G_CALLBACK(on_hsv_entry_activated));
    GtkWidget *saturation_box = create_scale_with_entry("Saturation", 0, 100, 1, &hsv_saturation_scale, &hsv_saturation_entry,
                                                        G_CALLBACK(on_hsv_scale_changed), G_CALLBACK(on_hsv_entry_activated));
    GtkWidget *value_box = create_scale_with_entry("Value", 0, 100, 1, &hsv_value_scale, &hsv_value_entry,
                                                   G_CALLBACK(on_hsv_scale_changed), G_CALLBACK(on_hsv_entry_activated));

    gtk_box_pack_start(GTK_BOX(box), hue_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), saturation_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), value_box, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(frame), box);
    return frame;
}

GtkWidget* create_preview_section() {
    GtkWidget *frame = gtk_frame_new("Color Preview & Picker");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    color_preview = gtk_drawing_area_new();
    gtk_widget_set_size_request(color_preview, 200, 100);
    gtk_widget_set_name(color_preview, "color-preview");

    color_picker_button = gtk_color_button_new();
    g_signal_connect(color_picker_button, "color-set", G_CALLBACK(on_color_picker_changed), NULL);

    gtk_box_pack_start(GTK_BOX(box), color_preview, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), gtk_label_new("Color Picker:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), color_picker_button, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(frame), box);
    return frame;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Color Models Converter");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_box_pack_start(GTK_BOX(main_box), create_rgb_section(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), create_cmyk_section(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), create_hsv_section(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), create_preview_section(), FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(window), main_box);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Установка начального цвета (красный)
    RGB initial_color(1.0, 0.0, 0.0);
    update_all_from_rgb(initial_color);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}