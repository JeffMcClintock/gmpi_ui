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

#include <cerrno>
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
        if (dragOffer_) wl_data_offer_destroy(dragOffer_);
        if (offer_)     wl_data_offer_destroy(offer_);
        if (source_)    wl_data_source_destroy(source_);
        if (device_)    wl_data_device_destroy(device_);
        if (queue_)     wl_event_queue_destroy(queue_);
    }

    // Everything clipboard lives on a queue of its OWN.
    //
    // Pasting has to dispatch while it waits - on a paste from our own selection
    // we are the source, and the bytes only appear when our send handler runs.
    // On the default queue that dispatch would also deliver whatever else was
    // pending, re-entering keyboard handling from inside a key handler: press
    // Ctrl+V then Return and the nested Return destroys the text edit that the
    // paste is about to write into. A private queue makes that impossible rather
    // than unlikely.
    void bind(wl_display* display, wl_data_device_manager* manager, wl_seat* seat)
    {
        static const wl_data_device_listener deviceListener = {
            dataOffer, deviceEnter, deviceLeave, deviceMotion, deviceDrop, selection
        };

        if (!display || !manager || !seat || device_)
            return;

        queue_ = wl_display_create_queue(display);
        if (!queue_)
            return;

        // A proxy inherits the queue of the proxy that created it, so the manager
        // is wrapped for the call and the wrapper thrown away immediately.
        auto* wrapped = static_cast<wl_data_device_manager*>(wl_proxy_create_wrapper(manager));
        if (!wrapped)
            return;
        wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(wrapped), queue_);

        manager_ = manager;
        device_  = wl_data_device_manager_get_data_device(wrapped, seat);
        wl_proxy_wrapper_destroy(reinterpret_cast<wl_proxy*>(wrapped));

        if (device_)
            wl_data_device_add_listener(device_, &deviceListener, this);
    }

    // Called from the app's normal loop, so selection events arrive even when
    // nobody is pasting.
    void pump(wl_display* display)
    {
        if (queue_ && display)
            wl_display_dispatch_queue_pending(display, queue_);
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

        auto* wrapped = static_cast<wl_data_device_manager*>(wl_proxy_create_wrapper(manager_));
        if (!wrapped)
            return;
        wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(wrapped), queue_);

        source_ = wl_data_device_manager_create_data_source(wrapped);
        wl_proxy_wrapper_destroy(reinterpret_cast<wl_proxy*>(wrapped));
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
            wl_display_flush(display);

            // Poll the display too, and dispatch. On a paste from OUR OWN
            // selection we are also the source: the bytes are only written when
            // our wl_data_source.send handler runs, so waiting on the pipe alone
            // waits for something that can never happen.
            pollfd p[2]{ { fds[0], POLLIN, 0 },
                         { wl_display_get_fd(display), POLLIN, 0 } };
            if (poll(p, 2, 200) <= 0)
                break;           // the other side went quiet; take what we have

            // OUR queue only. Dispatching the default queue here would run
            // keyboard and pointer handlers re-entrantly, from inside the key
            // handler that started this paste.
            if (p[1].revents & POLLIN)
                wl_display_dispatch_queue(display, queue_);

            if (p[0].revents & (POLLIN | POLLHUP))
            {
                const ssize_t n = read(fds[0], buf, sizeof(buf));
                if (n <= 0)
                    break;
                out.append(buf, size_t(n));

                if (out.size() > kMaxPaste)
                    break;       // a clipboard is not a file transfer
            }
        }

        close(fds[0]);
        return out;
    }

private:
    static constexpr size_t kMaxPaste = 4u * 1024 * 1024;
    static constexpr int    kSendBudgetMs = 2000;
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

    // We do not accept drops, but the offers still arrive and are ours to
    // destroy - one leaks per drag across the window otherwise.
    static void deviceEnter(void* data, wl_data_device*, uint32_t, wl_surface*,
                            wl_fixed_t, wl_fixed_t, wl_data_offer* offer)
    {
        auto& c = *static_cast<WaylandClipboard*>(data);
        if (c.dragOffer_)
            wl_data_offer_destroy(c.dragOffer_);
        c.dragOffer_ = offer;
    }
    static void deviceLeave(void* data, wl_data_device*)
    {
        auto& c = *static_cast<WaylandClipboard*>(data);
        if (c.dragOffer_)
        {
            wl_data_offer_destroy(c.dragOffer_);
            c.dragOffer_ = nullptr;
        }
    }
    static void deviceMotion(void*, wl_data_device*, uint32_t, wl_fixed_t, wl_fixed_t) {}
    static void deviceDrop(void* data, wl_data_device*) { deviceLeave(data, nullptr); }

    // --- wl_data_source: we are the one being asked for bytes ---
    static void sourceSend(void* data, wl_data_source*, const char*, int32_t fd)
    {
        auto& c = *static_cast<WaylandClipboard*>(data);

        // Non-blocking, with a budget. A pipe holds ~64K, so anything larger
        // waits for the other program to read - and a paster that stops reading
        // (or is stopped in a debugger) would otherwise hang OUR ui thread for
        // as long as it felt like.
        const int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0)
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        const char* p = c.pending_.data();
        size_t left = c.pending_.size();
        int budgetMs = kSendBudgetMs;

        while (left)
        {
            const ssize_t n = write(fd, p, left);
            if (n > 0)
            {
                p += n;
                left -= size_t(n);
                continue;
            }
            if (n < 0 && errno == EINTR)
                continue;
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                pollfd w{ fd, POLLOUT, 0 };
                const int r = poll(&w, 1, 50);
                budgetMs -= 50;
                if (r > 0 && (w.revents & POLLOUT))
                    continue;
                if (budgetMs <= 0)
                    break;              // they are not reading; give up, do not hang
                continue;
            }
            break;                      // EPIPE or a real error: they went away
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
    wl_data_offer*          dragOffer_{};
    wl_event_queue*         queue_{};

    std::string pending_;        // what we serve when asked
    std::string offerMime_;      // mime of the current selection
    std::string incomingMime_;   // best text mime seen on the offer being built
};

} // namespace wayland
} // namespace gmpi
