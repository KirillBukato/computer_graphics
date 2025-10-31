#include <gtkmm.h>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>

class ImageProcessor {
   public:
    ImageProcessor() : width(0), height(0) {}

    bool loadImage(const std::string& filename) {
        try {
            pixbuf = Gdk::Pixbuf::create_from_file(filename);
            if (!pixbuf) return false;

            width = pixbuf->get_width();
            height = pixbuf->get_height();

            originalPixbuf = pixbuf;
            filteredPixbuf = pixbuf->copy();

            return true;
        }
        catch (const Glib::Exception& ex) {
            return false;
        }
    }

    void applyLowPassFilter() {
        if (!filteredPixbuf) return;

        auto tempPixbuf = filteredPixbuf->copy();

        const int kernelSize = 3;
        int radius = kernelSize / 2;
        guint8* src_pixels = tempPixbuf->get_pixels();
        guint8* dst_pixels = filteredPixbuf->get_pixels();
        int rowstride = filteredPixbuf->get_rowstride();
        int n_channels = filteredPixbuf->get_n_channels();

        for (int y = radius; y < height - radius; ++y) {
            for (int x = radius; x < width - radius; ++x) {
                for (int channel = 0; channel < 3; channel++) {
                    int sum = 0;
                    int count = 0;

                    for (int ky = -radius; ky <= radius; ++ky) {
                        for (int kx = -radius; kx <= radius; ++kx) {
                            guint8* p = src_pixels + (y + ky) * rowstride + (x + kx) * n_channels;
                            sum += p[channel];
                            count++;
                        }
                    }

                    guint8* dst_p = dst_pixels + y * rowstride + x * n_channels;
                    dst_p[channel] = static_cast<guint8>(sum / count);
                }
            }
        }
    }

    std::vector<std::vector<int>> getHistogram() {
        std::vector<std::vector<int>> histogram(3, std::vector<int>(256, 0));

        if (!originalPixbuf) return histogram;

        guint8* pixels = originalPixbuf->get_pixels();
        int rowstride = originalPixbuf->get_rowstride();
        int n_channels = originalPixbuf->get_n_channels();

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                guint8* p = pixels + y * rowstride + x * n_channels;
                histogram[0][p[0]]++;
                histogram[1][p[1]]++;
                histogram[2][p[2]]++;
            }
        }

        return histogram;
    }

    void applyHistogramEqualization() {
        if (!originalPixbuf) return;

        filteredPixbuf = originalPixbuf->copy();

        auto histogram = getHistogram();

        std::vector<std::vector<int>> cdf(3, std::vector<int>(256, 0));
        int total_pixels = width * height;

        for (int channel = 0; channel < 3; channel++) {
            cdf[channel][0] = histogram[channel][0];
            for (int i = 1; i < 256; i++) {
                cdf[channel][i] = cdf[channel][i-1] + histogram[channel][i];
            }
        }

        std::vector<int> cdf_min(3, total_pixels);
        for (int channel = 0; channel < 3; channel++) {
            for (int i = 0; i < 256; i++) {
                if (histogram[channel][i] != 0) {
                    cdf_min[channel] = std::min(cdf_min[channel], cdf[channel][i]);
                }
            }
        }

        guint8* pixels = filteredPixbuf->get_pixels();
        int rowstride = filteredPixbuf->get_rowstride();
        int n_channels = filteredPixbuf->get_n_channels();

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                guint8* p = pixels + y * rowstride + x * n_channels;
                for (int channel = 0; channel < 3; channel++) {
                    int old_intensity = p[channel];
                    if (cdf[channel][old_intensity] > cdf_min[channel]) {
                        float equalized = (cdf[channel][old_intensity] - cdf_min[channel]) /
                                          static_cast<float>(total_pixels - cdf_min[channel]);
                        p[channel] = static_cast<guint8>(equalized * 255);
                    } else {
                        p[channel] = 0;
                    }
                }
            }
        }
    }

    void applyLinearContrast(int min_out = 0, int max_out = 255) {
        if (!originalPixbuf) return;

        filteredPixbuf = originalPixbuf->copy();

        std::vector<int> min_val(3, 255);
        std::vector<int> max_val(3, 0);

        guint8* orig_pixels = originalPixbuf->get_pixels();
        int rowstride = originalPixbuf->get_rowstride();
        int n_channels = originalPixbuf->get_n_channels();

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                guint8* p = orig_pixels + y * rowstride + x * n_channels;
                for (int channel = 0; channel < 3; channel++) {
                    min_val[channel] = std::min(min_val[channel], (int)p[channel]);
                    max_val[channel] = std::max(max_val[channel], (int)p[channel]);
                }
            }
        }

        guint8* pixels = filteredPixbuf->get_pixels();

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                guint8* p = pixels + y * rowstride + x * n_channels;
                for (int channel = 0; channel < 3; channel++) {
                    if (max_val[channel] != min_val[channel]) {
                        float normalized = static_cast<float>(p[channel] - min_val[channel]) /
                                           (max_val[channel] - min_val[channel]);
                        p[channel] = static_cast<guint8>(min_out + normalized * (max_out - min_out));
                    }
                }
            }
        }
    }

    std::vector<unsigned char> encodeRLE() {
        std::vector<unsigned char> encoded;
        if (!filteredPixbuf) return encoded;

        guint8* pixels = filteredPixbuf->get_pixels();
        int rowstride = filteredPixbuf->get_rowstride();
        int n_channels = filteredPixbuf->get_n_channels();

        encoded.push_back((width >> 8) & 0xFF);
        encoded.push_back(width & 0xFF);
        encoded.push_back((height >> 8) & 0xFF);
        encoded.push_back(height & 0xFF);

        for (int y = 0; y < height; ++y) {
            for (int channel = 0; channel < 3; channel++) {
                int count = 1;
                unsigned char current = pixels[y * rowstride + channel];

                for (int x = 1; x < width; ++x) {
                    unsigned char next = pixels[y * rowstride + x * n_channels + channel];
                    if (next == current && count < 255) {
                        count++;
                    } else {
                        encoded.push_back(count);
                        encoded.push_back(current);
                        current = next;
                        count = 1;
                    }
                }
                encoded.push_back(count);
                encoded.push_back(current);
            }
        }

        return encoded;
    }

    bool decodeRLE(const std::vector<unsigned char>& encoded) {
        if (encoded.size() < 4) return false;

        int decoded_width = (encoded[0] << 8) | encoded[1];
        int decoded_height = (encoded[2] << 8) | encoded[3];

        filteredPixbuf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, decoded_width, decoded_height);
        width = decoded_width;
        height = decoded_height;

        guint8* pixels = filteredPixbuf->get_pixels();
        int rowstride = filteredPixbuf->get_rowstride();
        int n_channels = filteredPixbuf->get_n_channels();

        size_t pos = 4;

        for (int y = 0; y < height && pos < encoded.size(); ++y) {
            for (int channel = 0; channel < 3 && pos < encoded.size(); channel++) {
                int x = 0;
                while (x < width && pos + 1 < encoded.size()) {
                    unsigned char count = encoded[pos++];
                    unsigned char value = encoded[pos++];

                    for (int i = 0; i < count && x < width; ++i) {
                        pixels[y * rowstride + x * n_channels + channel] = value;
                        x++;
                    }
                }
            }
        }

        return true;
    }

    bool saveRLEToFile(const std::string& filename) {
        auto encoded = encodeRLE();
        if (encoded.empty()) return false;

        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;

        file.write(reinterpret_cast<const char*>(encoded.data()), encoded.size());
        return true;
    }

    bool loadRLEFromFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<unsigned char> encoded(size);
        file.read(reinterpret_cast<char*>(encoded.data()), size);

        return decodeRLE(encoded);
    }

    void setOriginalFromFiltered() {
        if (filteredPixbuf) {
            originalPixbuf = filteredPixbuf->copy();
        }
    }

    Glib::RefPtr<Gdk::Pixbuf> getOriginalPixbuf() { return originalPixbuf; }
    Glib::RefPtr<Gdk::Pixbuf> getFilteredPixbuf() { return filteredPixbuf; }

    void resetToOriginal() {
        if (originalPixbuf) {
            filteredPixbuf = originalPixbuf->copy();
        }
    }

    bool hasImage() const { return (bool)originalPixbuf; }

   private:
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;
    Glib::RefPtr<Gdk::Pixbuf> originalPixbuf;
    Glib::RefPtr<Gdk::Pixbuf> filteredPixbuf;
    int width, height;
};

class HistogramDrawingArea : public Gtk::DrawingArea {
   public:
    HistogramDrawingArea(const std::vector<int>& histogram, const Gdk::RGBA& color)
            : histogram(histogram), color(color) {
        set_size_request(550, 300);
    }

   protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        Gtk::Allocation allocation = get_allocation();
        const int width = allocation.get_width();
        const int height = allocation.get_height();

        cr->set_source_rgb(1, 1, 1);
        cr->paint();

        int max_count = *std::max_element(histogram.begin(), histogram.end());
        if (max_count == 0) max_count = 1;

        const int margin = 50;
        const int graph_width = width - 2 * margin;
        const int graph_height = height - 2 * margin;

        cr->set_source_rgb(0, 0, 0);
        cr->set_line_width(2);
        cr->move_to(margin, margin);
        cr->line_to(margin, height - margin);
        cr->move_to(margin, height - margin);
        cr->line_to(width - margin, height - margin);
        cr->stroke();

        cr->set_source_rgba(color.get_red(), color.get_green(), color.get_blue(), 0.7);

        double bar_width = static_cast<double>(graph_width) / 256;
        for (int i = 0; i < 256; i++) {
            if (histogram[i] > 0) {
                double bar_height = (histogram[i] / static_cast<double>(max_count)) * graph_height;
                cr->rectangle(margin + i * bar_width,
                              height - margin - bar_height,
                              bar_width - 1,
                              bar_height);
                cr->fill();
            }
        }

        cr->set_source_rgb(0, 0, 0);
        cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
        cr->set_font_size(12);

        cr->save();
        cr->move_to(10, height / 2);
        cr->rotate(-M_PI / 2);
        cr->show_text("Frequency");
        cr->restore();

        cr->move_to(width / 2 - 30, height - 10);
        cr->show_text("Pixel Intensity");

        cr->set_font_size(14);
        cr->move_to(width / 2 - 40, 20);
        if (color.get_red() > 0.5) cr->show_text("Red Channel");
        else if (color.get_green() > 0.5) cr->show_text("Green Channel");
        else cr->show_text("Blue Channel");

        return true;
    }

   private:
    std::vector<int> histogram;
    Gdk::RGBA color;
};

class HistogramDialog : public Gtk::Dialog {
   public:
    HistogramDialog(Gtk::Window& parent, const std::vector<std::vector<int>>& histogram)
            : Gtk::Dialog("Image Histogram", parent, true) {

        set_default_size(600, 400);
        set_border_width(10);

        Gtk::Box* contentBox = get_content_area();

        notebook.append_page(redBox, "Red Channel");
        notebook.append_page(greenBox, "Green Channel");
        notebook.append_page(blueBox, "Blue Channel");

        redDrawingArea = Gtk::manage(new HistogramDrawingArea(histogram[0], Gdk::RGBA("red")));
        greenDrawingArea = Gtk::manage(new HistogramDrawingArea(histogram[1], Gdk::RGBA("green")));
        blueDrawingArea = Gtk::manage(new HistogramDrawingArea(histogram[2], Gdk::RGBA("blue")));

        redBox.pack_start(*redDrawingArea, true, true, 0);
        greenBox.pack_start(*greenDrawingArea, true, true, 0);
        blueBox.pack_start(*blueDrawingArea, true, true, 0);

        contentBox->pack_start(notebook, true, true, 0);

        add_button("_Close", Gtk::RESPONSE_CLOSE);

        show_all_children();

        redDrawingArea->queue_draw();
        greenDrawingArea->queue_draw();
        blueDrawingArea->queue_draw();
    }

   private:
    Gtk::Notebook notebook;
    Gtk::Box redBox{Gtk::ORIENTATION_VERTICAL};
    Gtk::Box greenBox{Gtk::ORIENTATION_VERTICAL};
    Gtk::Box blueBox{Gtk::ORIENTATION_VERTICAL};
    HistogramDrawingArea* redDrawingArea;
    HistogramDrawingArea* greenDrawingArea;
    HistogramDrawingArea* blueDrawingArea;
};

class MainWindow : public Gtk::Window {
   public:
    MainWindow() {
        set_title("Image Processing Tool");
        set_default_size(1000, 700);

        set_border_width(10);
        add(mainBox);

        setupMenu();
        setupImages();
        setupControls();

        show_all_children();
    }

   private:
    Gtk::Box mainBox{Gtk::ORIENTATION_VERTICAL};
    Gtk::Box imagesBox{Gtk::ORIENTATION_HORIZONTAL};
    Gtk::Box controlsBox{Gtk::ORIENTATION_VERTICAL};

    Gtk::ScrolledWindow originalScrolled, filteredScrolled;
    Gtk::Frame originalFrame, filteredFrame;
    Gtk::Image originalImage, filteredImage;

    Gtk::MenuBar menuBar;
    Gtk::Menu fileMenu, filterMenu, histogramMenu, compressionMenu;
    Gtk::MenuItem fileMenuItem, filterMenuItem, histogramMenuItem, compressionMenuItem;
    Gtk::MenuItem openMenuItem, saveMenuItem, exitMenuItem;
    Gtk::MenuItem lowpassMenuItem, equalizeMenuItem, contrastMenuItem, showHistogramMenuItem;
    Gtk::MenuItem encodeAndSaveRLEMenuItem, decodeAndOpenRLEMenuItem;

    Gtk::Label contrastMinLabel, contrastMaxLabel;
    Gtk::Scale contrastMinScale, contrastMaxScale;
    Gtk::Button lowpassButton, equalizeButton, contrastButton, showHistogramButton, resetButton, saveButton;
    Gtk::Button encodeAndSaveRLEButton, decodeAndOpenRLEButton;

    ImageProcessor processor;

    void setupMenu() {
        fileMenuItem.set_label("File");
        fileMenuItem.set_submenu(fileMenu);

        openMenuItem.set_label("Open Image");
        openMenuItem.signal_activate().connect([this]() { on_open_clicked(); });
        fileMenu.append(openMenuItem);

        saveMenuItem.set_label("Save Filtered Image");
        saveMenuItem.signal_activate().connect([this]() { on_save_clicked(); });
        fileMenu.append(saveMenuItem);

        fileMenu.append(*(new Gtk::SeparatorMenuItem()));

        exitMenuItem.set_label("Exit");
        exitMenuItem.signal_activate().connect([this]() { hide(); });
        fileMenu.append(exitMenuItem);

        filterMenuItem.set_label("Filter");
        filterMenuItem.set_submenu(filterMenu);

        lowpassMenuItem.set_label("Low-Pass Filter (3x3)");
        lowpassMenuItem.signal_activate().connect([this]() { on_lowpass_clicked(); });
        filterMenu.append(lowpassMenuItem);

        histogramMenuItem.set_label("Histogram");
        histogramMenuItem.set_submenu(histogramMenu);

        equalizeMenuItem.set_label("Histogram Equalization");
        equalizeMenuItem.signal_activate().connect([this]() { on_equalize_clicked(); });
        histogramMenu.append(equalizeMenuItem);

        contrastMenuItem.set_label("Linear Contrast");
        contrastMenuItem.signal_activate().connect([this]() { on_contrast_clicked(); });
        histogramMenu.append(contrastMenuItem);

        showHistogramMenuItem.set_label("Show Histogram");
        showHistogramMenuItem.signal_activate().connect([this]() { on_show_histogram_clicked(); });
        histogramMenu.append(showHistogramMenuItem);

        compressionMenuItem.set_label("RLE");
        compressionMenuItem.set_submenu(compressionMenu);

        encodeAndSaveRLEMenuItem.set_label("Encode and Save RLE");
        encodeAndSaveRLEMenuItem.signal_activate().connect([this]() { on_encode_and_save_rle_clicked(); });
        compressionMenu.append(encodeAndSaveRLEMenuItem);

        decodeAndOpenRLEMenuItem.set_label("Decode and Open RLE");
        decodeAndOpenRLEMenuItem.signal_activate().connect([this]() { on_decode_and_open_rle_clicked(); });
        compressionMenu.append(decodeAndOpenRLEMenuItem);

        menuBar.append(fileMenuItem);
        menuBar.append(filterMenuItem);
        menuBar.append(histogramMenuItem);
        menuBar.append(compressionMenuItem);

        mainBox.pack_start(menuBar, Gtk::PACK_SHRINK);
    }

    void setupImages() {
        imagesBox.set_spacing(10);
        imagesBox.set_border_width(5);
        mainBox.pack_start(imagesBox, true, true, 0);

        originalFrame.set_label("Original Image");
        originalScrolled.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
        originalScrolled.set_size_request(400, 400);
        originalScrolled.add(originalImage);
        originalFrame.add(originalScrolled);
        imagesBox.pack_start(originalFrame, true, true, 0);

        filteredFrame.set_label("Filtered Image");
        filteredScrolled.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
        filteredScrolled.set_size_request(400, 400);
        filteredScrolled.add(filteredImage);
        filteredFrame.add(filteredScrolled);
        imagesBox.pack_start(filteredFrame, true, true, 0);
    }

    void setupControls() {
        controlsBox.set_spacing(10);
        controlsBox.set_border_width(10);

        Gtk::Frame lowpassFrame("Low-Pass Filter");
        Gtk::Box lowpassBox{Gtk::ORIENTATION_HORIZONTAL};
        lowpassBox.set_spacing(10);
        lowpassBox.set_border_width(5);

        lowpassButton.set_label("Apply 3x3 Low-Pass Filter");
        lowpassButton.signal_clicked().connect([this]() { on_lowpass_clicked(); });
        lowpassBox.pack_start(lowpassButton, Gtk::PACK_SHRINK);

        lowpassFrame.add(lowpassBox);
        controlsBox.pack_start(lowpassFrame, Gtk::PACK_SHRINK);

        Gtk::Frame histogramFrame("Histogram Operations");
        Gtk::Box histogramControlsBox{Gtk::ORIENTATION_HORIZONTAL};
        histogramControlsBox.set_spacing(10);
        histogramControlsBox.set_border_width(5);

        equalizeButton.set_label("Equalize Histogram");
        equalizeButton.signal_clicked().connect([this]() { on_equalize_clicked(); });
        histogramControlsBox.pack_start(equalizeButton, Gtk::PACK_SHRINK);

        showHistogramButton.set_label("Show Histogram");
        showHistogramButton.signal_clicked().connect([this]() { on_show_histogram_clicked(); });
        histogramControlsBox.pack_start(showHistogramButton, Gtk::PACK_SHRINK);

        histogramFrame.add(histogramControlsBox);
        controlsBox.pack_start(histogramFrame, Gtk::PACK_SHRINK);

        Gtk::Frame contrastFrame("Linear Contrast");
        Gtk::Box contrastBox{Gtk::ORIENTATION_HORIZONTAL};
        contrastBox.set_spacing(10);
        contrastBox.set_border_width(5);

        contrastMinLabel.set_label("Min Output:");
        contrastBox.pack_start(contrastMinLabel, Gtk::PACK_SHRINK);

        contrastMinScale.set_range(0, 254);
        contrastMinScale.set_value(0);
        contrastMinScale.set_size_request(80, -1);
        contrastBox.pack_start(contrastMinScale, Gtk::PACK_SHRINK);

        contrastMaxLabel.set_label("Max Output:");
        contrastBox.pack_start(contrastMaxLabel, Gtk::PACK_SHRINK);

        contrastMaxScale.set_range(1, 255);
        contrastMaxScale.set_value(255);
        contrastMaxScale.set_size_request(80, -1);
        contrastBox.pack_start(contrastMaxScale, Gtk::PACK_SHRINK);

        contrastButton.set_label("Apply Contrast");
        contrastButton.signal_clicked().connect([this]() { on_contrast_clicked(); });
        contrastBox.pack_start(contrastButton, Gtk::PACK_SHRINK);

        contrastFrame.add(contrastBox);
        controlsBox.pack_start(contrastFrame, Gtk::PACK_SHRINK);

        Gtk::Frame compressionFrame("RLE Compression");
        Gtk::Box compressionBox{Gtk::ORIENTATION_HORIZONTAL};
        compressionBox.set_spacing(10);
        compressionBox.set_border_width(5);

        encodeAndSaveRLEButton.set_label("Encode and Save RLE");
        encodeAndSaveRLEButton.signal_clicked().connect([this]() { on_encode_and_save_rle_clicked(); });
        compressionBox.pack_start(encodeAndSaveRLEButton, Gtk::PACK_SHRINK);

        decodeAndOpenRLEButton.set_label("Decode and Open RLE");
        decodeAndOpenRLEButton.signal_clicked().connect([this]() { on_decode_and_open_rle_clicked(); });
        compressionBox.pack_start(decodeAndOpenRLEButton, Gtk::PACK_SHRINK);

        compressionFrame.add(compressionBox);
        controlsBox.pack_start(compressionFrame, Gtk::PACK_SHRINK);

        Gtk::Box commonBox{Gtk::ORIENTATION_HORIZONTAL};
        commonBox.set_spacing(10);
        commonBox.set_border_width(5);

        resetButton.set_label("Reset to Original");
        resetButton.signal_clicked().connect([this]() { on_reset_clicked(); });
        commonBox.pack_start(resetButton, Gtk::PACK_SHRINK);

        saveButton.set_label("Save Result");
        saveButton.signal_clicked().connect([this]() { on_save_clicked(); });
        commonBox.pack_start(saveButton, Gtk::PACK_SHRINK);

        controlsBox.pack_start(commonBox, Gtk::PACK_SHRINK);

        mainBox.pack_start(controlsBox, Gtk::PACK_SHRINK);
    }

    void on_open_clicked() {
        Gtk::FileChooserDialog dialog("Choose an image", Gtk::FILE_CHOOSER_ACTION_OPEN);
        dialog.set_transient_for(*this);

        dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("_Open", Gtk::RESPONSE_OK);

        auto filter_image = Gtk::FileFilter::create();
        filter_image->set_name("Image files");
        filter_image->add_mime_type("image/jpeg");
        filter_image->add_mime_type("image/png");
        filter_image->add_mime_type("image/bmp");
        filter_image->add_pattern("*.jpg");
        filter_image->add_pattern("*.jpeg");
        filter_image->add_pattern("*.png");
        filter_image->add_pattern("*.bmp");
        dialog.add_filter(filter_image);

        if (dialog.run() == Gtk::RESPONSE_OK) {
            std::string filename = dialog.get_filename();
            if (processor.loadImage(filename)) {
                updateImages();
            }
        }
    }

    void on_save_clicked() {
        if (!processor.hasImage()) return;

        Gtk::FileChooserDialog dialog("Save image", Gtk::FILE_CHOOSER_ACTION_SAVE);
        dialog.set_transient_for(*this);

        dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("_Save", Gtk::RESPONSE_OK);

        auto filter_png = Gtk::FileFilter::create();
        filter_png->set_name("PNG files");
        filter_png->add_mime_type("image/png");
        filter_png->add_pattern("*.png");
        dialog.add_filter(filter_png);

        dialog.set_current_name("processed_image.png");

        if (dialog.run() == Gtk::RESPONSE_OK) {
            std::string filename = dialog.get_filename();
            if (!filename.empty()) {
                processor.getFilteredPixbuf()->save(filename, "png");
            }
        }
    }

    void on_lowpass_clicked() {
        if (!processor.hasImage()) return;
        processor.applyLowPassFilter();
        updateImages();
    }

    void on_equalize_clicked() {
        if (!processor.hasImage()) return;
        processor.applyHistogramEqualization();
        updateImages();
    }

    void on_contrast_clicked() {
        if (!processor.hasImage()) return;

        int min_out = static_cast<int>(contrastMinScale.get_value());
        int max_out = static_cast<int>(contrastMaxScale.get_value());

        if (min_out >= max_out) return;

        processor.applyLinearContrast(min_out, max_out);
        updateImages();
    }

    void on_show_histogram_clicked() {
        if (!processor.hasImage()) return;

        auto histogram = processor.getHistogram();
        HistogramDialog dialog(*this, histogram);
        dialog.run();
    }

    void on_encode_and_save_rle_clicked() {
        if (!processor.hasImage()) return;

        Gtk::FileChooserDialog dialog("Save RLE", Gtk::FILE_CHOOSER_ACTION_SAVE);
        dialog.set_transient_for(*this);

        dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("_Save", Gtk::RESPONSE_OK);

        auto filter_rle = Gtk::FileFilter::create();
        filter_rle->set_name("RLE files");
        filter_rle->add_pattern("*.rle");
        dialog.add_filter(filter_rle);

        dialog.set_current_name("image.rle");

        if (dialog.run() == Gtk::RESPONSE_OK) {
            std::string filename = dialog.get_filename();
            if (!filename.empty()) {
                if (processor.saveRLEToFile(filename)) {
                    Gtk::MessageDialog success(*this, "RLE saved successfully", false, Gtk::MESSAGE_INFO);
                    success.run();
                }
            }
        }
    }

    void on_decode_and_open_rle_clicked() {
        Gtk::FileChooserDialog dialog("Load RLE", Gtk::FILE_CHOOSER_ACTION_OPEN);
        dialog.set_transient_for(*this);

        dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("_Open", Gtk::RESPONSE_OK);

        auto filter_rle = Gtk::FileFilter::create();
        filter_rle->set_name("RLE files");
        filter_rle->add_pattern("*.rle");
        dialog.add_filter(filter_rle);

        if (dialog.run() == Gtk::RESPONSE_OK) {
            std::string filename = dialog.get_filename();
            if (processor.loadRLEFromFile(filename)) {
                processor.setOriginalFromFiltered();
                updateImages();
                Gtk::MessageDialog success(*this, "RLE loaded as original image", false, Gtk::MESSAGE_INFO);
                success.run();
            }
        }
    }

    void on_reset_clicked() {
        if (!processor.hasImage()) return;
        processor.resetToOriginal();
        updateImages();
    }

    void updateImages() {
        if (processor.hasImage()) {
            auto original = processor.getOriginalPixbuf();
            auto filtered = processor.getFilteredPixbuf();

            if (original) {
                originalImage.set(original);
            }

            if (filtered) {
                filteredImage.set(filtered);
            }
        }
    }
};

int main(int argc, char** argv) {
    auto app = Gtk::Application::create(argc, argv, "org.gtkmm.imageprocessor");

    MainWindow window;

    return app->run(window);
}