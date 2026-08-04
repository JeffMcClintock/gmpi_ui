// A small app driving the REAL Wayland backend, so the automated harness
// exercises shipping code rather than a parallel implementation.
//
// Everything the backend offers that needs a compositor to prove - popup menus
// with submenus and real grabs, message boxes, portal file dialogs - is reachable
// from here with ordinary input, which means autotest.sh can drive it unattended:
//
//     right click        context menu, with a submenu on "Arrange"
//     m                  message box (Yes/No/Cancel)
//     o / s              file dialog, open / save
//     Escape             quit
//
// build: ./tests/run.sh --demo

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>

#include "backends/DrawingFrameWayland.h"
#include "helpers/NativeUi.h"
#include "helpers/BundledFonts.h"
#include "helpers/CpuTextEngine.h"
#include "helpers/DecodeImage.h"
#include "helpers/FontProvider.h"
#include "Drawing.h"

using namespace gmpi;

namespace
{

class DemoClient : public api::IDrawingClient, public api::IInputClient
{
public:
    explicit DemoClient(wayland::WaylandToplevel& frame) : frame_(frame) {}

    void setFont(drawing::api::ITextFormat* f) { font_ = f; }
    const std::string& status() const { return status_; }

    // --- IDrawingClient / IInputClient share setHost ---
    ReturnCode setHost(api::IUnknown* host) override
    {
        host_ = host;
        return ReturnCode::Ok;
    }

    ReturnCode measure(const drawing::Size*, drawing::Size* returnDesiredSize) override
    {
        *returnDesiredSize = { 700.f, 460.f };
        return ReturnCode::Ok;
    }
    ReturnCode arrange(const drawing::Rect* finalRect) override
    {
        bounds_ = *finalRect;
        return ReturnCode::Ok;
    }
    ReturnCode getClipArea(drawing::Rect* returnRect) override
    {
        *returnRect = bounds_;
        return ReturnCode::Ok;
    }
    ReturnCode render(drawing::api::IDeviceContext* dc) override;

    // --- IInputClient ---
    ReturnCode setHover(bool h) override
    {
        printf("setHover(%d)\n", int(h)); fflush(stdout);
        return ReturnCode::Ok;
    }
    ReturnCode hitTest(drawing::Point, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onPointerDown(drawing::Point p, int32_t) override
    {
        status_ = "pointer down at " + std::to_string(int(p.x)) + "," + std::to_string(int(p.y));
        printf("%s\n", status_.c_str());
        fflush(stdout);
        invalidate();
        return ReturnCode::Ok;
    }
    ReturnCode onPointerMove(drawing::Point, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onPointerUp(drawing::Point, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onMouseWheel(drawing::Point, int32_t, int32_t) override { return ReturnCode::Unhandled; }
    ReturnCode getToolTip(drawing::Point, api::IString*) override { return ReturnCode::Unhandled; }

    ReturnCode populateContextMenu(drawing::Point, api::IUnknown* sink) override;
    ReturnCode onKeyPress(wchar_t c) override;

    ReturnCode queryInterface(const api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(api::IDrawingClient);
        GMPI_QUERYINTERFACE(api::IInputClient);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT_NO_DELETE;   // lives on the stack in main()

private:
    void invalidate()
    {
        api::IDrawingHost* h{};
        if (host_ && host_->queryInterface(&api::IDrawingHost::guid,
                                           reinterpret_cast<void**>(&h)) == ReturnCode::Ok)
        {
            h->invalidateRect(nullptr);
            h->release();
        }
    }

    void showMessageBox();
    void showFileDialog(bool save);
    void showColorDialog();
    void showTextEdit();

    wayland::WaylandToplevel& frame_;
    api::IUnknown* host_{};
    drawing::api::ITextFormat* font_{};
    drawing::Rect bounds_{ 0, 0, 700, 460 };
    std::string status_ = "right-click = menu;  m = message box;  o/s = file;  k = colour;  t = edit;  Esc = quit";
};

ReturnCode DemoClient::render(drawing::api::IDeviceContext* dc)
{
    static int n = 0;
    if (++n <= 2) { printf("render #%d\n", n); fflush(stdout); }

    const drawing::Color bg{ 0.12f, 0.13f, 0.15f, 1.f };
    dc->clear(&bg);

    if (!font_)
        return ReturnCode::Ok;

    drawing::api::ISolidColorBrush* ink{};
    const drawing::Color fg{ 0.90f, 0.91f, 0.93f, 1.f };
    dc->createSolidColorBrush(&fg, nullptr, &ink);
    if (!ink)
        return ReturnCode::Ok;

    const drawing::Rect r{ 20.f, 20.f, bounds_.right - 20.f, 60.f };
    dc->drawTextU(status_.c_str(), uint32_t(status_.size()), font_, &r, ink, 0);

    ink->release();
    return ReturnCode::Ok;
}

ReturnCode DemoClient::populateContextMenu(drawing::Point, api::IUnknown* sinkUnknown)
{
    // shared_ptr's raw-pointer constructor ATTACHES, so wrapping a borrowed
    // pointer would free the menu out from under the backend.
    api::IContextItemSink* sinkRaw{};
    if (!sinkUnknown || sinkUnknown->queryInterface(&api::IContextItemSink::guid,
                                                    reinterpret_cast<void**>(&sinkRaw)) != ReturnCode::Ok)
        return ReturnCode::Unhandled;
    shared_ptr<api::IContextItemSink> sink;
    sink.attach(sinkRaw);

    printf("context menu requested\n");
    fflush(stdout);

    using F = api::PopupMenuFlags;
    sink->addItem("Insert Module", 1, 0, nullptr);
    sink->addItem("Paste", 2, int32_t(F::Grayed), nullptr);
    sink->addItem("", 0, int32_t(F::Separator), nullptr);
    sink->addItem("Show Grid", 3, int32_t(F::Ticked), nullptr);
    sink->addItem("Arrange", 0, int32_t(F::SubMenuBegin), nullptr);
    sink->addItem("Align Left", 10, 0, nullptr);
    sink->addItem("Align Top", 11, 0, nullptr);
    sink->addItem("", 0, int32_t(F::Separator), nullptr);
    sink->addItem("Distribute", 12, 0, nullptr);
    sink->addItem("", 0, int32_t(F::SubMenuEnd), nullptr);
    sink->addItem("", 0, int32_t(F::Separator), nullptr);
    sink->addItem("Properties...", 4, 0, nullptr);
    return ReturnCode::Ok;
}

ReturnCode DemoClient::onKeyPress(wchar_t c)
{
    switch (c)
    {
    case 'm': case 'M': showMessageBox();     break;
    case 'o': case 'O': showFileDialog(false); break;
    case 's': case 'S': showFileDialog(true);  break;
    case 'k': case 'K': showColorDialog();     break;
    case 't': case 'T': showTextEdit();        break;
    case 27:            frame_.close();        break;   // Escape
    default: return ReturnCode::Unhandled;
    }
    return ReturnCode::Ok;
}

void DemoClient::showMessageBox()
{
    api::IUnknown* raw{};
    if (frame_.createStockDialog(int32_t(api::StockDialogType::YesNoCancel),
                                 "SynthEdit", "The project has unsaved changes.\n"
                                 "Save before closing?", &raw) != ReturnCode::Ok || !raw)
    {
        printf("message box: not supported\n");
        return;
    }

    shared_ptr<api::IUnknown> owner;
    owner.attach(raw);

    auto dlg = owner.as<api::IStockDialog>();
    if (!dlg)
        return;

    // The dialog is async, so the callback must outlive this call.
    static sdk::StockDialogCallback cb([this](api::StockDialogButton b)
    {
        static const char* names[] = { "Ok", "Cancel", "Yes", "No" };
        status_ = std::string("message box -> ") + names[int(b) & 3];
        printf("%s\n", status_.c_str());
        invalidate();
    });
    dlg->showAsync(&cb);
}

void DemoClient::showFileDialog(bool save)
{
    api::IUnknown* raw{};
    if (frame_.createFileDialog(save ? 1 : 0, &raw) != ReturnCode::Ok || !raw)
    {
        printf("file dialog: not supported\n");
        return;
    }

    shared_ptr<api::IUnknown> owner;
    owner.attach(raw);

    auto dlg = owner.as<api::IFileDialog>();
    if (!dlg)
        return;

    dlg->addExtension("se1", "SynthEdit Project");
    dlg->setInitialFilename(save ? "Untitled.se1" : "");

    printf("file dialog requested (%s)\n", save ? "save" : "open");
    fflush(stdout);

    static sdk::FileDialogCallback cb(
        [this](const std::string& path)
        {
            status_ = "file -> " + path;
            printf("%s\n", status_.c_str());
            invalidate();
        },
        [this]()
        {
            status_ = "file dialog cancelled";
            printf("%s\n", status_.c_str());
            invalidate();
        });
    if (dlg->showAsync(nullptr, &cb) != ReturnCode::Ok)
    {
        printf("file dialog: showAsync failed (no session bus or no portal)\n");
        fflush(stdout);
    }
}

void DemoClient::showColorDialog()
{
    api::IUnknown* raw{};
    if (frame_.createColorDialog(drawing::Color{ 0.216f, 0.05f, 0.05f, 1.f }, &raw)
            != ReturnCode::Ok || !raw)
    {
        printf("colour dialog: not supported\n");
        fflush(stdout);
        return;
    }

    shared_ptr<api::IUnknown> owner;
    owner.attach(raw);

    auto dlg = owner.as<api::IColorDialog>();
    if (!dlg)
        return;

    printf("colour dialog opened\n");
    fflush(stdout);

    static sdk::ColorDialogCallback cb([this](drawing::Color c)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "colour -> %.3f %.3f %.3f a%.2f", c.r, c.g, c.b, c.a);
        status_ = buf;
        printf("%s\n", status_.c_str());
        fflush(stdout);
        invalidate();
    });
    dlg->showAsync(&cb);
}

void DemoClient::showTextEdit()
{
    api::IUnknown* raw{};
    const drawing::Rect box{ 40.f, 220.f, 320.f, 248.f };
    if (frame_.createTextEdit(&box, &raw) != ReturnCode::Ok || !raw)
    {
        printf("text edit: not supported\n");
        fflush(stdout);
        return;
    }

    shared_ptr<api::IUnknown> owner;
    owner.attach(raw);

    auto edit = owner.as<api::ITextEdit>();
    if (!edit)
        return;

    edit->setText("Oscillator 1");
    edit->setTextSize(14.f);

    printf("text edit opened\n");
    fflush(stdout);

    static sdk::TextEditCallback cb(
        [this](const std::string& text)
        {
            status_ = "edit committed -> " + text;
            printf("%s\n", status_.c_str());
            fflush(stdout);
            invalidate();
        },
        [this]()
        {
            status_ = "edit cancelled";
            printf("%s\n", status_.c_str());
            fflush(stdout);
            invalidate();
        });
    edit->showAsync(&cb);
}

} // namespace

int main(int argc, char** argv)
{
    int maxFrames = 0;
    for (int i = 1; i < argc; ++i)
        if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            maxFrames = atoi(argv[++i]);

    wayland::Connection connection;
    if (!connection.open())
    {
        fprintf(stderr, "no wayland display\n");
        return 1;
    }

    wayland::WaylandToplevel frame(connection);

    // The CPU backend deliberately has no font or shaping code, so wire in the
    // same pieces every other host does.
    static drawing::CpuTextEngine textEngine{ drawing::findFont };
    textEngine.imageDecoder             = drawing::decodeImageMemory;
    frame.drawingFactory().imageDecoder = drawing::decodeImageFile;
    frame.drawingFactory().textEngine   = &textEngine;

    drawing::Factory facade;
    *drawing::AccessPtr::put(facade) = &frame.drawingFactory();

    const std::filesystem::path fonts =
        std::filesystem::path(getenv("HOME") ? getenv("HOME") : ".")
        / "SE/SE16/SynthEdit2/Resources/fonts";
    drawing::registerBundledFont("Selawik", drawing::FontWeight::Regular,
                                 drawing::FontStyle::Normal, (fonts / "selawk.ttf").string());

    const std::string_view family{ "Selawik" };
    auto font = facade.createTextFormat(14.0f, std::span{ &family, 1 });
    if (!font)
        fprintf(stderr, "warning: no font; text will not draw\n");

    frame.setMenuFont(drawing::AccessPtr::get(font));

    if (!frame.create("gmpi_ui wayland demo", "gmpi_ui.demo", 700, 460))
    {
        fprintf(stderr, "could not create window\n");
        return 1;
    }

    DemoClient client(frame);
    client.setFont(drawing::AccessPtr::get(font));
    frame.attachClient(&client);

    int frames = 0;
    frame.runEventLoop(16, [&](int)
    {
        if (maxFrames && ++frames >= maxFrames)
            frame.close();
    });

    printf("demo exited cleanly\n");
    return 0;
}
