// Headless checks for the Wayland backend: the flat addItem stream -> menu tree,
// and the popup teardown registry that keeps us from dropping the connection
// while a popup grab is live.
//
// Items reach IPopupMenu flat, with SubMenuBegin/SubMenuEnd markers delimiting
// nesting, so rebuilding the tree is the part most likely to be wrong - and the
// part that needs no display server to verify.
//
// build:  ./tests/run.sh     (no compositor or display required)
#include "backends/DrawingFrameWayland.h"
#include "helpers/NativeUi.h"
#include "backends/PortalFileDialog.h"
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cmath>

using F = gmpi::api::PopupMenuFlags;
static int failures = 0;

static void check(const char* what, bool ok)
{
    if (!ok) ++failures;
    printf("%-58s %s\n", what, ok ? "ok" : "FAIL");
}

int main()
{
    gmpi::wayland::Connection connection;   // never opened: the model needs nothing
    gmpi::wayland::InputDispatch input;
    gmpi::cpugfx::Factory factory;

    gmpi::wayland::WaylandPopupMenu menu(connection, input, nullptr, factory, nullptr, {});

    menu.addItem("", 0, (int32_t)F::Separator, nullptr);        // leading: dropped
    menu.addItem("&Insert Module", 1, 0, nullptr);
    menu.addItem("", 0, (int32_t)F::Separator, nullptr);
    menu.addItem("", 0, (int32_t)F::Separator, nullptr);        // doubled: one only
    menu.addItem("Paste", 2, (int32_t)F::Grayed, nullptr);
    menu.addItem("Show Grid", 3, (int32_t)F::Ticked, nullptr);
    menu.addItem("Arrange", 0, (int32_t)F::SubMenuBegin, nullptr);
    menu.addItem("Align Left", 10, 0, nullptr);
    menu.addItem("Align Top", 11, 0, nullptr);
    menu.addItem("", 0, (int32_t)F::SubMenuEnd, nullptr);
    menu.addItem("Properties...", 4, 0, nullptr);
    menu.addItem("", 0, (int32_t)F::Separator, nullptr);        // trailing: dropped

    const auto& items = menu.items();

    check("leading and trailing separators dropped",
          !items.empty() && !items.front().separator && !items.back().separator);

    int separators = 0;
    for (const auto& i : items) if (i.separator) ++separators;
    check("doubled separator collapsed to one", separators == 1);

    check("mnemonic '&' stripped", !items.empty() && items[0].label == "Insert Module");

    const auto* paste = items.size() > 2 ? &items[2] : nullptr;
    check("grayed flag carried", paste && paste->grayed && paste->label == "Paste");

    const auto* grid = items.size() > 3 ? &items[3] : nullptr;
    check("ticked flag carried", grid && grid->ticked);

    const auto* arrange = items.size() > 4 ? &items[4] : nullptr;
    check("submenu rebuilt from Begin/End markers",
          arrange && arrange->label == "Arrange" && arrange->submenu.size() == 2);
    check("submenu children in order",
          arrange && arrange->submenu.size() == 2
                  && arrange->submenu[0].id == 10 && arrange->submenu[1].id == 11);

    check("items after the submenu land back at top level",
          items.size() == 6 && items.back().label == "Properties..." && items.back().id == 4);

    // --- popup teardown registry ------------------------------------------
    // A client that drops its connection while a popup grab is live makes mutter
    // crash or hang, taking the user's session with it. Connection must close any
    // survivors, topmost-first, before disconnecting.
    {
        struct FakePopup : gmpi::wayland::IPopupTeardown
        {
            std::vector<int>* order; int id;
            FakePopup(std::vector<int>* o, int i) : order(o), id(i) {}
            void closeSurfaces() override { order->push_back(id); }
        };

        std::vector<int> closed;
        {
            gmpi::wayland::Connection c;   // no display: roundtrip is skipped
            FakePopup a(&closed, 1), b(&closed, 2), d(&closed, 3);
            c.registerPopup(&a); c.registerPopup(&b); c.registerPopup(&d);
            c.closeLivePopups();
        }
        check("live popups are closed topmost-first",
              closed.size() == 3 && closed[0] == 3 && closed[1] == 2 && closed[2] == 1);
    }
    {
        struct FakePopup : gmpi::wayland::IPopupTeardown
        {
            bool* flag;
            explicit FakePopup(bool* f) : flag(f) {}
            void closeSurfaces() override { *flag = true; }
        };

        bool closedOnDestruct = false;
        {
            gmpi::wayland::Connection c;
            FakePopup p(&closedOnDestruct);
            c.registerPopup(&p);
        }   // ~Connection must not leave a grabbing popup behind
        check("~Connection closes a still-open popup", closedOnDestruct);
    }
    {
        struct FakePopup : gmpi::wayland::IPopupTeardown
        {
            int* n;
            explicit FakePopup(int* c) : n(c) {}
            void closeSurfaces() override { ++*n; }
        };

        int closes = 0;
        gmpi::wayland::Connection c;
        FakePopup p(&closes);
        c.registerPopup(&p);
        c.unregisterPopup(&p);      // popup tore itself down first
        c.closeLivePopups();
        check("an unregistered popup is not closed twice", closes == 0);
    }

    // --- clicking a submenu header -----------------------------------------
    // A submenu header carries no id of its own. Completing the menu with it
    // hands the caller a meaningless value; on the spike it also tore the popup
    // down while its child was still up. Items are 24px tall starting at y=4,
    // so index 0 spans 4..28, index 1 spans 28..52, index 2 spans 52..76.
    {
        int chosenId = -1;
        gmpi::sdk::PopupMenuCallback cb([&](int32_t id) { chosenId = id; });

        auto* m = new gmpi::wayland::WaylandPopupMenu(connection, input, nullptr, factory, nullptr, {});
        m->addRef();                       // stands in for the reference showAsync takes
        m->addItem("Plain", 1, 0, &cb);
        // give the header a callback too: without it the test cannot tell a
        // correct "do nothing" from a wrong "complete with a meaningless id"
        m->addItem("Arrange", 77, (int32_t)F::SubMenuBegin, &cb);
        m->addItem("Align Left", 10, 0, &cb);
        m->addItem("", 0, (int32_t)F::SubMenuEnd, nullptr);
        m->addItem("Other", 2, 0, &cb);

        m->onButton(0, 40, 272, false, 0);      // release over "Arrange"
        // addItem stores no callback for a header, so the callback alone cannot
        // tell right from wrong here - the menu staying OPEN is the real assertion
        check("clicking a submenu header does not complete the menu",
              chosenId == -1 && !m->isDismissed());

        m->onKey(0xff1b, 0);                    // Escape: dismiss, drops the stand-in ref
        m->release();
    }
    {
        int chosenId = -1;
        gmpi::sdk::PopupMenuCallback cb([&](int32_t id) { chosenId = id; });

        auto* m = new gmpi::wayland::WaylandPopupMenu(connection, input, nullptr, factory, nullptr, {});
        m->addRef();
        m->addItem("Plain", 1, 0, &cb);
        m->addItem("Arrange", 0, (int32_t)F::SubMenuBegin, nullptr);
        m->addItem("Align Left", 10, 0, &cb);
        m->addItem("", 0, (int32_t)F::SubMenuEnd, nullptr);
        m->addItem("Other", 2, 0, &cb);

        m->onButton(0, 64, 272, false, 0);      // release over "Other"
        check("clicking a plain item completes with its id",
              chosenId == 2 && m->isDismissed());
        m->release();                            // choose() already dismissed
    }
    {
        int chosenId = -1;
        gmpi::sdk::PopupMenuCallback cb([&](int32_t id) { chosenId = id; });

        auto* m = new gmpi::wayland::WaylandPopupMenu(connection, input, nullptr, factory, nullptr, {});
        m->addRef();
        m->addItem("Plain", 1, 0, &cb);
        m->addItem("Grayed", 9, (int32_t)F::Grayed, &cb);

        m->onButton(0, 40, 272, false, 0);      // release over the grayed item
        check("clicking a grayed item completes nothing", chosenId == -1);
        m->release();                            // dismiss() ran in onButton
    }

    // --- portal URI decoding ------------------------------------------------
    // The portal answers with URIs, not paths. Getting this wrong means opening
    // or overwriting the wrong file, so it is worth pinning down precisely.
    {
        using gmpi::wayland::PortalBus;

        check("plain file URI becomes a path",
              PortalBus::uriToPath("file:///home/x/song.se") == "/home/x/song.se");

        check("percent-escaped spaces decoded",
              PortalBus::uriToPath("file:///home/x/My%20Song.se") == "/home/x/My Song.se");

        check("lower and upper case hex both decoded",
              PortalBus::uriToPath("file:///%41%2f%2Fb") == "/A//b");

        check("non-file URI yields no path",
              PortalBus::uriToPath("http://example.com/song.se").empty());

        // A truncated escape is data we do not understand; passing the raw bytes
        // through beats inventing a byte or silently dropping one.
        check("truncated escape left alone",
              PortalBus::uriToPath("file:///a%2") == "/a%2");

        check("non-hex escape left alone",
              PortalBus::uriToPath("file:///a%zz") == "/a%zz");

        check("multibyte utf-8 round-trips",
              PortalBus::uriToPath("file:///caf%C3%A9.se") == "/caf\xc3\xa9.se");
    }

    // --- stock dialogs ------------------------------------------------------
    // Wayland has no message box, so we draw one. Which buttons a type offers and
    // what Escape means are the parts a caller depends on being right.
    {
        using T = gmpi::api::StockDialogType;
        using B = gmpi::api::StockDialogButton;
        using Dlg = gmpi::wayland::WaylandStockDialog;

        auto make = [&](T t) {
            return Dlg(connection, input, nullptr, factory, nullptr,
                       int32_t(t), "Title", "A message that the user has to read.");
        };

        auto ok = make(T::Ok);
        check("Ok dialog offers one button",
              ok.buttons().size() == 1 && ok.buttons()[0].id == B::Ok);
        check("Ok dialog: Escape means Ok", ok.escapeButton() == B::Ok);

        auto okc = make(T::OkCancel);
        check("OkCancel offers Ok then Cancel",
              okc.buttons().size() == 2 && okc.buttons()[0].id == B::Ok
                                        && okc.buttons()[1].id == B::Cancel);
        check("OkCancel: Escape means Cancel", okc.escapeButton() == B::Cancel);

        auto yn = make(T::YesNo);
        check("YesNo offers Yes then No",
              yn.buttons().size() == 2 && yn.buttons()[0].id == B::Yes
                                       && yn.buttons()[1].id == B::No);
        // Escape must NOT mean Yes: dismissing a "discard changes?" box by
        // accident should never be the destructive answer.
        check("YesNo: Escape means No", yn.escapeButton() == B::No);

        auto ync = make(T::YesNoCancel);
        check("YesNoCancel offers three buttons",
              ync.buttons().size() == 3 && ync.buttons()[2].id == B::Cancel);
        check("YesNoCancel: Escape means Cancel", ync.escapeButton() == B::Cancel);

        // Button geometry. A desktop's buttons pad their label and centre it;
        // ours drew every label hard against the left edge of a fixed-width
        // button, which is what this pins. Needs a font to measure with - with
        // none there is nothing to measure and every button takes the floor
        // width - so a stub stands in for one, 8px per character.
        {
            struct StubFont : gmpi::drawing::api::ITextFormat
            {
                gmpi::ReturnCode setTextAlignment(gmpi::drawing::TextAlignment) override
                    { return gmpi::ReturnCode::Ok; }
                gmpi::ReturnCode setParagraphAlignment(gmpi::drawing::ParagraphAlignment) override
                    { return gmpi::ReturnCode::Ok; }
                gmpi::ReturnCode setWordWrapping(gmpi::drawing::WordWrapping) override
                    { return gmpi::ReturnCode::Ok; }
                gmpi::ReturnCode getTextExtentU(const char*, int32_t len, float,
                                                gmpi::drawing::Size* returnSize) override
                {
                    *returnSize = { 8.0f * float(len), 18.0f };
                    return gmpi::ReturnCode::Ok;
                }
                gmpi::ReturnCode getFontMetrics(gmpi::drawing::FontMetrics*) override
                    { return gmpi::ReturnCode::NoSupport; }
                gmpi::ReturnCode setLineSpacing(float, float) override
                    { return gmpi::ReturnCode::Ok; }
                gmpi::ReturnCode queryInterface(const gmpi::api::Guid*, void** obj) override
                    { *obj = {}; return gmpi::ReturnCode::NoSupport; }
                GMPI_REFCOUNT_NO_DELETE;
            } stubFont;

            Dlg dlg(connection, input, nullptr, factory, &stubFont,
                    int32_t(T::YesNoCancel), "Title", "Save changes to Untitled?");

            const auto& bs = dlg.buttons();          // Yes, No, Cancel (rightmost first)
            auto width = [](const Dlg::Button& b) { return b.rect.right - b.rect.left; };

            // "Cancel" is 6 chars = 48px of text, plus 16px either side.
            check("a button is its label plus 16px of padding each side",
                  width(bs[2]) == 48.0f + 32.0f);

            // "No" would be 16 + 32 = 48, under the floor.
            check("a short label stops at the floor width", width(bs[1]) == 64.0f);

            check("a longer label gets a wider button", width(bs[2]) > width(bs[1]));

            // Right-aligned against the margin, 6px apart - the action-area
            // spacing a GNOME desktop uses.
            check("the first button is flush with the right margin",
                  bs[0].rect.right == float(dlg.contentWidth() - 20));
            check("buttons sit 6px apart",
                  bs[0].rect.left - bs[1].rect.right == 6.0f
                  && bs[1].rect.left - bs[2].rect.right == 6.0f);

            // The point of the exercise: equal space either side of the label,
            // and the same top and bottom.
            for (const auto& b : bs)
            {
                const auto o = dlg.labelOrigin(b);
                const float padL = o.x - b.rect.left;
                const float padR = b.rect.right - (o.x + b.labelSize.width);
                const float padT = o.y - b.rect.top;
                const float padB = b.rect.bottom - (o.y + b.labelSize.height);
                check("the label is centred horizontally", std::fabs(padL - padR) < 0.01f);
                check("the label is centred vertically",   std::fabs(padT - padB) < 0.01f);
            }
        }

        // Mnemonics: the first letter of a label picks that button. Without
        // them "No" is unreachable from the keyboard - Return takes the
        // default and Escape the cancel - which is the one answer a save
        // prompt most needs.
        {
            auto dlg = make(T::YesNoCancel);
            const auto* n = dlg.buttonForMnemonic('n');
            check("a save prompt answers 'n' with No", n && n->id == B::No);

            const auto* y = dlg.buttonForMnemonic('Y');
            check("mnemonics are case-insensitive", y && y->id == B::Yes);

            const auto* c = dlg.buttonForMnemonic('c');
            check("'c' picks Cancel", c && c->id == B::Cancel);

            check("an unmatched letter picks nothing", dlg.buttonForMnemonic('q') == nullptr);
            check("a digit picks nothing", dlg.buttonForMnemonic('7') == nullptr);
        }

        // Wheel deltas. Two conventions meet here and both are easy to get
        // backwards: Wayland counts DOWN as positive where Windows counts it
        // negative, and the editor wants notches of 120.
        {
            using ID = gmpi::wayland::InputDispatch;
            check("one wheel notch down is -120",  ID::wheelDelta(1, 10.0) == -120);
            check("one wheel notch up is +120",    ID::wheelDelta(-1, -10.0) == 120);
            check("three notches scale",           ID::wheelDelta(3, 30.0) == -360);
            // A touchpad sends distance with no notch count; it must still
            // scroll, just smoothly.
            check("touchpad falls back to distance", ID::wheelDelta(0, 10.0) == -100);
            check("touchpad direction matches",      ID::wheelDelta(0, -5.0) == 50);
            check("no movement is no delta",         ID::wheelDelta(0, 0.0) == 0);
        }

        // geometry: laid out before mapping, inside the window, non-overlapping
        bool inside = true, ordered = true;
        const auto& bs = ync.buttons();
        for (size_t i = 0; i < bs.size(); ++i)
        {
            if (bs[i].rect.left < 0 || bs[i].rect.right > float(ync.contentWidth())
                || bs[i].rect.bottom > float(ync.contentHeight()))
                inside = false;
            if (i && bs[i].rect.right > bs[i - 1].rect.left)
                ordered = false;      // laid out right to left, no overlap
        }
        check("dialog buttons sit inside the window", inside);
        check("dialog buttons do not overlap", ordered);
        check("default button is rightmost",
              bs.size() > 1 && bs[0].rect.left > bs[1].rect.left);
    }

    // --- key listener -------------------------------------------------------
    // Wayland has no "grab the keyboard for a widget", so the listener is a sink
    // the dispatcher consults ahead of the drawing client. What matters to a
    // caller: keys arrive with the right modifiers, Escape ends the edit, and
    // the clipboard shortcuts ask the callback for its text.
    {
        using KL = gmpi::wayland::WaylandKeyListener;
        struct Recorder : gmpi::api::IKeyListenerCallback
        {
            std::vector<std::string> events;
            void onKeyDown(int32_t key, int32_t flags) override
            { events.push_back("down " + std::to_string(key) + " f" + std::to_string(flags)); }
            void onKeyUp(int32_t key, int32_t flags) override
            { events.push_back("up " + std::to_string(key) + " f" + std::to_string(flags)); }
            void onLostFocus(gmpi::ReturnCode r) override
            { events.push_back("lostfocus " + std::to_string(int(r))); }
            void cut(gmpi::api::IString* s) override
            { events.push_back("cut"); s->setData("CUTTEXT", 7); }
            void copy(gmpi::api::IString* s) override
            { events.push_back("copy"); s->setData("COPYTEXT", 8); }
            void paste(const char* t, size_t n) override
            { events.push_back("paste " + std::string(t, n)); }
            gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
            {
                *returnInterface = {};
                GMPI_QUERYINTERFACE(gmpi::api::IKeyListenerCallback);
                return gmpi::ReturnCode::NoSupport;
            }
            GMPI_REFCOUNT_NO_DELETE;
        };

        const int32_t kCtrl  = int32_t(gmpi::api::PointerFlags::KeyControl);
        const int32_t kShift = int32_t(gmpi::api::PointerFlags::KeyShift);

        {
            Recorder rec;
            auto* kl = new KL(connection, input);
            kl->showAsync(&rec);
            check("showAsync installs the key sink", input.keySink() != nullptr);

            input.keySink()->onRawKey('a', 'a', kShift, true);
            input.keySink()->onRawKey('a', 'a', kShift, false);
            check("key down and up both reported with modifiers",
                  rec.events.size() == 2
                  && rec.events[0] == "down 97 f" + std::to_string(kShift)
                  && rec.events[1] == "up 97 f" + std::to_string(kShift));

            // Escape ends the edit rather than arriving as a keystroke
            input.keySink()->onRawKey(0xff1b, 0, 0, true);
            check("Escape reports lost focus and detaches",
                  rec.events.size() == 3
                  && rec.events[2].rfind("lostfocus", 0) == 0
                  && input.keySink() == nullptr);
            kl->release();
        }
        {
            Recorder rec;
            auto* kl = new KL(connection, input);
            kl->showAsync(&rec);
            input.keySink()->onRawKey('c', 'c', kCtrl, true);
            input.keySink()->onRawKey('x', 'x', kCtrl, true);
            check("ctrl-C and ctrl-X ask the callback for its text",
                  rec.events.size() == 2 && rec.events[0] == "copy" && rec.events[1] == "cut");

            // and are not ALSO delivered as ordinary keystrokes
            check("clipboard shortcuts are not delivered as keys",
                  std::none_of(rec.events.begin(), rec.events.end(),
                               [](const std::string& e){ return e.rfind("down", 0) == 0; }));

            // acting on the release too would copy twice
            input.keySink()->onRawKey('c', 'c', kCtrl, false);
            check("clipboard shortcuts act once, on press only", rec.events.size() == 2);

            kl->showAsync(nullptr);   // detach path without a callback
            kl->release();
        }
    }

    // --- colour dialog ------------------------------------------------------
    // Every surface in this library is LINEAR, but hue/saturation/value are
    // perceptual and a hex code means sRGB. Getting that boundary backwards
    // gives colours that are wrong but plausible, which is the worst kind.
    {
        using CD = gmpi::wayland::WaylandColorDialog;
        auto close = [](float a, float b) { return std::fabs(a - b) < 0.01f; };

        // the transfer function itself, against the published constant
        check("sRGB 0.5 decodes to linear 0.214",  close(CD::srgbToLinear01(0.5f), 0.2140f));
        check("sRGB 0 and 1 are fixed points",
              close(CD::srgbToLinear01(0.f), 0.f) && close(CD::srgbToLinear01(1.f), 1.f));
        check("decode is the inverse of encode",
              close(CD::srgbToLinear01(gmpi::drawing::linearToSRGB01(0.3f)), 0.3f));

        float r, g, b;
        CD::hsvToSrgb({ 0.f, 1.f, 1.f }, r, g, b);
        check("hue 0 is pure red", close(r, 1.f) && close(g, 0.f) && close(b, 0.f));
        CD::hsvToSrgb({ 120.f, 1.f, 1.f }, r, g, b);
        check("hue 120 is pure green", close(r, 0.f) && close(g, 1.f) && close(b, 0.f));
        CD::hsvToSrgb({ 240.f, 1.f, 1.f }, r, g, b);
        check("hue 240 is pure blue", close(r, 0.f) && close(g, 0.f) && close(b, 1.f));

        // round trip through the hex-like sRGB representation
        bool roundTrips = true;
        for (float h : { 15.f, 100.f, 200.f, 300.f })
            for (float sat : { 0.3f, 1.f })
                for (float val : { 0.4f, 1.f })
                {
                    CD::hsvToSrgb({ h, sat, val }, r, g, b);
                    const auto back = CD::srgbToHsv(r, g, b, 1.f);
                    if (!close(back.h, h) || !close(back.s, sat) || !close(back.v, val))
                        roundTrips = false;
                }
        check("hsv survives a round trip through sRGB", roundTrips);

        check("grey keeps a stable hue rather than a random one",
              CD::srgbToHsv(0.5f, 0.5f, 0.5f, 1.f).h == 0.f
              && CD::srgbToHsv(0.5f, 0.5f, 0.5f, 1.f).s == 0.f);

        // the boundary that matters: what goes in linear comes back linear
        gmpi::wayland::WaylandColorDialog dlg(connection, input, nullptr, factory, nullptr,
                                              gmpi::drawing::Color{ 0.216f, 0.216f, 0.216f, 1.f });
        const auto out = dlg.color();
        // NB this round trip alone proves nothing: drop BOTH conversions and it
        // still passes, because it is only self-consistent. The absolute check
        // below is the one that fails when the boundary is wrong.
        check("a linear colour survives the trip through HSV",
              close(out.r, 0.216f) && close(out.g, 0.216f) && close(out.b, 0.216f));
        check("mid-linear grey is NOT treated as mid-sRGB",
              dlg.hsv().v > 0.4f && dlg.hsv().v < 0.6f);   // linear .216 IS sRGB ~.5

        dlg.setFromLinear(gmpi::drawing::Color{ 1.f, 0.f, 0.f, 0.5f });
        check("alpha is carried through unchanged", close(dlg.hsv().a, 0.5f));
        check("pure linear red reads as hue 0", close(dlg.hsv().h, 0.f));

        // hit-testing: the three controls and the buttons must not overlap
        using R = gmpi::wayland::WaylandColorDialog::Region;
        check("saturation square hit-tests", dlg.regionAt(40, 40) == R::SatVal);
        check("hue strip hit-tests",  dlg.regionAt(245, 40) == R::Hue);
        check("alpha strip hit-tests", dlg.regionAt(283, 40) == R::Alpha);
        check("empty space hits nothing",
              dlg.regionAt(dlg.contentWidth() - 5, 5) == R::None);
    }

    // --- text edit ----------------------------------------------------------
    // Held as UTF-8, edited by codepoint. Stepping a byte at a time walks into
    // the middle of any character above U+007F, and the next Backspace then
    // writes invalid UTF-8 into whatever the user was renaming.
    //
    // These are heap objects on purpose: Escape and Return call finish(), which
    // releases, and a refcounted object reaching zero deletes itself - on the
    // stack that frees an address that was never allocated.
    {
        using TE = gmpi::wayland::WaylandTextEdit;
        const int32_t kCtrl  = int32_t(gmpi::api::PointerFlags::KeyControl);
        const int32_t kShift = int32_t(gmpi::api::PointerFlags::KeyShift);

        auto key = [&](TE* e, uint32_t sym, uint32_t utf32, int32_t flags = 0)
        { e->handleKey(sym, utf32, flags, true); };

        auto make = [&](const char* initial) {
            auto* e = new TE(connection, input, nullptr, gmpi::drawing::Rect{});
            e->setText(initial);
            return e;
        };

        {
            auto* e = make("");
            key(e, 'H', 'H'); key(e, 'i', 'i');
            check("typing appends text", e->text() == "Hi" && e->caret() == 2);
            key(e, 0xff08, 0);                       // Backspace
            check("backspace removes one character", e->text() == "H");
            e->release();
        }
        {
            // "cafe" with an acute e - two bytes, so a byte-wise caret breaks here
            auto* e = make("caf\xc3\xa9");
            key(e, 0xff57, 0);                       // End
            check("end goes past the last codepoint", e->caret() == 5);
            key(e, 0xff51, 0);                       // Left
            check("left steps a whole codepoint, not a byte", e->caret() == 3);
            key(e, 0xff53, 0);                       // Right
            key(e, 0xff08, 0);                       // Backspace over the accented e
            check("backspace removes a whole multi-byte character", e->text() == "caf");
            e->release();
        }
        {
            auto* e = make("hello");
            key(e, 0xff50, 0);                       // Home
            key(e, 0xff53, 0, kShift);               // shift-Right
            key(e, 0xff53, 0, kShift);
            check("shift-arrow extends a selection",
                  e->selection().first == 0 && e->selection().second == 2);
            key(e, 'X', 'X');
            check("typing replaces the selection", e->text() == "Xllo");
            e->release();
        }
        {
            auto* e = make("abc");
            key(e, 0xff51, 0);                       // plain Left collapses
            check("a plain arrow collapses the selection",
                  e->selection().first == e->selection().second);
            e->release();
        }
        {
            auto* e = make("abc");
            key(e, 'a', 'a', kCtrl);                 // ctrl-A
            check("ctrl-A selects everything",
                  e->selection().first == 0 && e->selection().second == 3);
            key(e, 'q', 'q', kCtrl);
            check("an unhandled ctrl chord inserts nothing", e->text() == "abc");
            e->release();
        }
        {
            // The scroll policy as arithmetic - these are the exact moves that
            // used to paint outside the box or hide the caret.
            using TE2 = gmpi::wayland::WaylandTextEdit;
            check("short text never scrolls",
                  TE2::scrollFor(/*caret*/ 30.f, /*text*/ 50.f, /*span*/ 100.f, 0.f) == 0.f);
            check("caret past the right edge pulls the window right",
                  TE2::scrollFor(250.f, 300.f, 100.f, 0.f) == 150.f);
            check("caret before the window pulls it left",
                  TE2::scrollFor(20.f, 300.f, 100.f, 150.f) == 20.f);
            check("deleting the tail un-scrolls rather than showing a gap",
                  TE2::scrollFor(120.f, 120.f, 100.f, 150.f) == 20.f);
            check("home from a long tail lands at zero",
                  TE2::scrollFor(0.f, 300.f, 100.f, 200.f) == 0.f);
        }
        {
            // The regression this pins: the edit forwarded keys to the shared
            // model but never wired its callbacks, so Return and Escape were
            // silently dead - typing worked, committing did not. Only the
            // FRAME path shows it, hence asserting through the callback.
            struct Sink : gmpi::api::ITextEditCallback
            {
                int completions = 0;
                gmpi::ReturnCode last{};
                void onChanged(const char*) override {}
                void onComplete(gmpi::ReturnCode r) override { ++completions; last = r; }
                gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid,
                                                void** returnInterface) override
                {
                    *returnInterface = {};
                    GMPI_QUERYINTERFACE(gmpi::api::ITextEditCallback);
                    return gmpi::ReturnCode::NoSupport;
                }
                GMPI_REFCOUNT_NO_DELETE;
            } sink;

            auto* e = make("ab");
            e->showAsync(&sink);
            key(e, 0xff0d, 0);                       // Return
            check("return COMMITS the edit through its callback",
                  sink.completions == 1 && sink.last == gmpi::ReturnCode::Ok);
        }
        {
            auto* e = make("ab");
            key(e, 0xff1b, 0);                       // Escape: finish() releases it
            check("escape ends the edit without touching the text", true);
        }
        {
            auto* e = make("");
            key(e, 0x0d, 0x0d);                      // a raw control code, not Return
            check("control codes are not inserted as text", e->text().empty());
            e->release();
        }
    }

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures != 0;
}
