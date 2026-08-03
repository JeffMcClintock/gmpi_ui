#pragma once
/*
  The system clipboard on Wayland.

  There is no clipboard API: a selection is a wl_data_source the client owns and
  serves on demand. Copying means offering MIME types and then WRITING the bytes
  down a pipe when some other client asks; pasting means being handed a
  wl_data_offer, asking for a MIME type, and READING the bytes back off a pipe.

  That asymmetry is why this is ~200 lines instead of two calls, and why setting
  the selection needs a real input serial - the compositor only lets the client
  that most recently handled input take ownership of the clipboard.
*/

#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <wayland-client.h>

namespace gmpi
{
namespace wayland
{

class WaylandClipboard
{
public:
    ~WaylandClipboard()
    {
        if (source_) wl_data_source_destroy(source_);
        if (device_) wl_data_device_destroy(device_);
    }

    void bind(wl_data_device_manager* manager, wl_seat* seat)
    {
        static const wl_data_device_listener deviceListener = {
            dataOffer, deviceEnter, deviceLeave, deviceMotion, deviceDrop, selection
        };

        if (!manager || !seat || device_)
            return;

        manager_ = manager;
        device_  = wl_data_device_manager_get_data_device(manager, seat);
        if (device_)
            wl_data_device_add_listener(device_, &deviceListener, this);
    }

    bool available() const { return device_ != nullptr; }

    // Take ownership of the selection. The serial must be from a real input event
    // or the compositor ignores us; there is no error, the copy just does nothing.
    void setText(wl_display* display, const std::string& text, uint32_t serial)
    {
        static const wl_data_source_listener sourceListener = {
            sourceTarget, sourceSend, sourceCancelled,
            sourceDndDropPerformed, sourceDndFinished, sourceAction
        };

        if (!manager_ || !device_)
            return;

        if (source_)
        {
            wl_data_source_destroy(source_);
            source_ = nullptr;
        }

        pending_ = text;

        source_ = wl_data_device_manager_create_data_source(manager_);
        if (!source_)
            return;

        wl_data_source_add_listener(source_, &sourceListener, this);
        for (const char* m : kTextMimes)
            wl_data_source_offer(source_, m);

        wl_data_device_set_selection(device_, source_, serial);
        if (display)
            wl_display_flush(display);
    }

    // Read the current selection. Genuinely round-trips through a pipe, so it is
    // bounded by a timeout rather than left to block the UI thread forever if the
    // owning client never writes.
    std::string getText(wl_display* display)
    {
        if (!offer_ || offerMime_.empty() || !display)
            return {};

        int fds[2];
        if (pipe2(fds, O_CLOEXEC) != 0)
            return {};

        wl_data_offer_receive(offer_, offerMime_.c_str(), fds[1]);
        close(fds[1]);           // our copy: the reader must see EOF when they finish
        wl_display_flush(display);

        std::string out;
        char buf[4096];
        for (;;)
        {
            pollfd p{ fds[0], POLLIN, 0 };
            if (poll(&p, 1, 200) <= 0)
                break;           // the other side went quiet; take what we have

            const ssize_t n = read(fds[0], buf, sizeof(buf));
            if (n <= 0)
                break;
            out.append(buf, size_t(n));

            if (out.size() > kMaxPaste)
                break;           // a clipboard is not a file transfer
        }

        close(fds[0]);
        return out;
    }

private:
    static constexpr size_t kMaxPaste = 4u * 1024 * 1024;
    static constexpr const char* kTextMimes[] = {
        "text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "STRING", "TEXT"
    };

    static bool isTextMime(const char* m)
    {
        for (const char* k : kTextMimes)
            if (m && 0 == strcmp(m, k))
                return true;
        return false;
    }

    // --- wl_data_offer ---
    static void offerMime(void* data, wl_data_offer*, const char* mime)
    {
        auto& c = *static_cast<WaylandClipboard*>(data);

        // prefer the explicitly-UTF8 flavour; fall back to whatever text we get
        if (!isTextMime(mime))
            return;
        if (c.incomingMime_.empty() || 0 == strcmp(mime, kTextMimes[0]))
            c.incomingMime_ = mime;
    }
    static void offerSourceActions(void*, wl_data_offer*, uint32_t) {}
    static void offerAction(void*, wl_data_offer*, uint32_t) {}

    static void dataOffer(void* data, wl_data_device*, wl_data_offer* offer)
    {
        static const wl_data_offer_listener offerListener = {
            offerMime, offerSourceActions, offerAction
        };

        auto& c = *static_cast<WaylandClipboard*>(data);
        c.incomingMime_.clear();
        wl_data_offer_add_listener(offer, &offerListener, &c);
    }

    // The offer becomes the selection only here; before this it is just a
    // candidate, and its mime list has been arriving in the meantime.
    static void selection(void* data, wl_data_device*, wl_data_offer* offer)
    {
        auto& c = *static_cast<WaylandClipboard*>(data);

        if (c.offer_)
            wl_data_offer_destroy(c.offer_);

        c.offer_     = offer;
        c.offerMime_ = offer ? c.incomingMime_ : std::string{};
    }

    static void deviceEnter(void*, wl_data_device*, uint32_t, wl_surface*,
                            wl_fixed_t, wl_fixed_t, wl_data_offer*) {}
    static void deviceLeave(void*, wl_data_device*) {}
    static void deviceMotion(void*, wl_data_device*, uint32_t, wl_fixed_t, wl_fixed_t) {}
    static void deviceDrop(void*, wl_data_device*) {}

    // --- wl_data_source: we are the one being asked for bytes ---
    static void sourceSend(void* data, wl_data_source*, const char*, int32_t fd)
    {
        auto& c = *static_cast<WaylandClipboard*>(data);

        // Write in a loop: a pipe can take less than we offer, and a paster that
        // gives up mid-read leaves us with EPIPE rather than an error.
        const char* p = c.pending_.data();
        size_t left = c.pending_.size();
        while (left)
        {
            const ssize_t n = write(fd, p, left);
            if (n <= 0)
                break;
            p += n;
            left -= size_t(n);
        }
        close(fd);
    }
    static void sourceCancelled(void* data, wl_data_source* src)
    {
        // someone else took the clipboard; our source is dead
        auto& c = *static_cast<WaylandClipboard*>(data);
        wl_data_source_destroy(src);
        if (c.source_ == src)
            c.source_ = nullptr;
    }
    static void sourceTarget(void*, wl_data_source*, const char*) {}
    static void sourceDndDropPerformed(void*, wl_data_source*) {}
    static void sourceDndFinished(void*, wl_data_source*) {}
    static void sourceAction(void*, wl_data_source*, uint32_t) {}

    wl_data_device_manager* manager_{};
    wl_data_device*         device_{};
    wl_data_source*         source_{};
    wl_data_offer*          offer_{};

    std::string pending_;        // what we serve when asked
    std::string offerMime_;      // mime of the current selection
    std::string incomingMime_;   // best text mime seen on the offer being built
};

} // namespace wayland
} // namespace gmpi
