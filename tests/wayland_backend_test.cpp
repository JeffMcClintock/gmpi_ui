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

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures != 0;
}
