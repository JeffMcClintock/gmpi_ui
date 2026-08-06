#pragma once

// ---------------------------------------------------------------------------
// The CLIPBOARD selection, enough of it for a rename box.
//
// X11 has no clipboard: it has selections, and the owner serves the data on
// demand. So copying means claiming ownership and answering SelectionRequest
// for as long as we hold it, and pasting means asking the current owner and
// waiting for a SelectionNotify.
//
// Two consequences worth knowing:
//
//  * Copied text dies with the plugin. Nothing here talks to a clipboard
//    manager, so text copied out of a plugin and pasted after the plugin closes
//    is gone. Every toolkit needs xclipboard/klipper for that too.
//
//  * getText() is the one place that waits. A plugin may not run an event loop,
//    but a paste cannot be asynchronous behind ITextEdit's synchronous
//    interface, so it pumps the display for a bounded time and gives up rather
//    than hanging the DAW. Requests for OUR selection are answered while
//    waiting, so two plugins pasting from each other cannot deadlock.
// ---------------------------------------------------------------------------

#include <chrono>
#include <thread>
#include <string>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

namespace gmpi::hosting
{

class X11Clipboard
{
public:
    void init(Display* display, Window owner)
    {
        display_ = display;
        owner_ = owner;
        if (!display_)
            return;

        clipboard_ = XInternAtom(display_, "CLIPBOARD", False);
        utf8_      = XInternAtom(display_, "UTF8_STRING", False);
        targets_   = XInternAtom(display_, "TARGETS", False);
        property_  = XInternAtom(display_, "GMPI_CLIP", False);
    }

    void setText(const std::string& text)
    {
        if (!display_ || !owner_)
            return;

        owned_ = text;
        XSetSelectionOwner(display_, clipboard_, owner_, CurrentTime);
        XFlush(display_);
    }

    // Blocks, briefly and boundedly - see the note above.
    std::string getText()
    {
        if (!display_ || !owner_)
            return {};

        // We own it: skip the round trip entirely. This is also what stops a
        // request to ourselves from waiting for a reply we would have to be
        // idle to send.
        if (XGetSelectionOwner(display_, clipboard_) == owner_)
            return owned_;

        XConvertSelection(display_, clipboard_, utf8_, property_, owner_, CurrentTime);
        XFlush(display_);

        using namespace std::chrono;
        const auto deadline = steady_clock::now() + milliseconds(200);

        while (steady_clock::now() < deadline)
        {
            XEvent e{};
            if (!XCheckTypedWindowEvent(display_, owner_, SelectionNotify, &e))
            {
                // Keep serving our own selection while we wait, or two plugins
                // pasting from one another would each sit waiting for the other.
                XEvent other{};
                if (XCheckTypedWindowEvent(display_, owner_, SelectionRequest, &other))
                    handleRequest(other.xselectionrequest);
                else
                    std::this_thread::sleep_for(milliseconds(2));
                continue;
            }

            if (e.xselection.property == 0)
                return {};   // the owner cannot give us UTF-8

            Atom type{};
            int format = 0;
            unsigned long items = 0, remaining = 0;
            unsigned char* data = nullptr;

            if (XGetWindowProperty(display_, owner_, property_, 0, 1 << 20, True,
                                   AnyPropertyType, &type, &format, &items, &remaining,
                                   &data) != Success || !data)
                return {};

            std::string result(reinterpret_cast<char*>(data), size_t(items));
            XFree(data);
            return result;
        }

        return {};   // nobody answered; better an empty paste than a frozen host
    }

    // Called from the frame's event pump for events naming our window.
    bool handleEvent(const XEvent& e)
    {
        if (!display_)
            return false;

        if (e.type == SelectionRequest && e.xselectionrequest.owner == owner_)
        {
            handleRequest(e.xselectionrequest);
            return true;
        }

        // Someone else took the selection; drop our copy so a later paste asks
        // them rather than serving stale text.
        if (e.type == SelectionClear && e.xselectionclear.selection == clipboard_)
        {
            owned_.clear();
            return true;
        }

        return false;
    }

private:
    void handleRequest(const XSelectionRequestEvent& req)
    {
        XSelectionEvent reply{};
        reply.type = SelectionNotify;
        reply.display = req.display;
        reply.requestor = req.requestor;
        reply.selection = req.selection;
        reply.target = req.target;
        reply.time = req.time;
        reply.property = req.property ? req.property : req.target;

        if (req.target == targets_)
        {
            const Atom offered[] = { targets_, utf8_, XA_STRING };
            XChangeProperty(display_, req.requestor, reply.property, XA_ATOM, 32,
                            PropModeReplace, reinterpret_cast<const unsigned char*>(offered),
                            int(sizeof offered / sizeof offered[0]));
        }
        else if (req.target == utf8_ || req.target == XA_STRING)
        {
            XChangeProperty(display_, req.requestor, reply.property, req.target, 8,
                            PropModeReplace,
                            reinterpret_cast<const unsigned char*>(owned_.data()),
                            int(owned_.size()));
        }
        else
        {
            reply.property = 0;   // we cannot supply that target
        }

        XSendEvent(display_, req.requestor, False, 0, reinterpret_cast<XEvent*>(&reply));
        XFlush(display_);
    }

    Display* display_{};
    Window   owner_{};
    Atom     clipboard_{}, utf8_{}, targets_{}, property_{};
    std::string owned_;
};

} // namespace gmpi::hosting
