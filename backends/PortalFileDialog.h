#pragma once
/*
  IFileDialog on xdg-desktop-portal.

  Wayland has no file chooser: a client cannot position a window, cannot know
  where its own window is, and must not draw one the compositor would have to
  place blind. The desktop's answer is the FileChooser portal over D-Bus, which
  runs the dialog in the portal process and hands back URIs. That also means the
  dialog is sandboxed-app-safe and themed like the rest of the desktop.

  The awkward part is the reply. OpenFile returns an object path, but the actual
  answer arrives later as a Request::Response SIGNAL on that path - so the match
  rule has to be installed BEFORE the call is sent, which means predicting the
  path from our unique bus name and a token we choose. That prediction is exactly
  what the portal documentation tells clients to do.

  Everything here is non-blocking: pump() is driven from the same poll loop as
  wayland, so a file dialog never nests a modal loop. Under Wayland a client that
  blocks while holding a popup grab wedges the compositor.
*/

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <dbus/dbus.h>

#include "helpers/NativeUi.h"
#include "GmpiApiCommon.h"
#include "RefCountMacros.h"

namespace gmpi
{
namespace wayland
{

// ---------------------------------------------------------------------------
// PortalBus - one session-bus connection, shared by every portal dialog
// ---------------------------------------------------------------------------
class PortalBus
{
public:
    struct Waiter
    {
        virtual void onPortalResponse(uint32_t response, const std::vector<std::string>& uris) = 0;
    protected:
        ~Waiter() = default;
    };

    ~PortalBus()
    {
        if (bus_)
            dbus_connection_unref(bus_);
    }

    bool connect()
    {
        if (bus_)
            return true;

        DBusError err;
        dbus_error_init(&err);

        bus_ = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err))
        {
            dbus_error_free(&err);
            return false;
        }
        if (!bus_)
            return false;

        // we drive the loop ourselves; libdbus must not kill the host process
        dbus_connection_set_exit_on_disconnect(bus_, FALSE);
        return true;
    }

    DBusConnection* connection() const { return bus_; }

    // Poll this alongside the wayland fd. -1 when there is nothing to watch.
    int fd() const
    {
        int f = -1;
        if (bus_ && dbus_connection_get_unix_fd(bus_, &f))
            return f;
        return -1;
    }

    bool busy() const { return !waiters_.empty(); }

    // "/org/freedesktop/portal/desktop/request/<unique name>/<token>", with the
    // unique name's leading ':' dropped and its dots turned into underscores.
    std::string predictPath(const std::string& token) const
    {
        const char* u = bus_ ? dbus_bus_get_unique_name(bus_) : nullptr;
        std::string unique = u ? u : "";
        if (!unique.empty() && unique[0] == ':')
            unique.erase(0, 1);
        for (auto& c : unique)
            if (c == '.') c = '_';

        return "/org/freedesktop/portal/desktop/request/" + unique + "/" + token;
    }

    std::string nextToken()
    {
        return "gmpi" + std::to_string(++counter_);
    }

    // The match must be in place before the method call goes out, or the reply
    // can beat it and the signal is dropped.
    bool watch(const std::string& requestPath, Waiter* w)
    {
        if (!bus_)
            return false;

        DBusError err;
        dbus_error_init(&err);

        const std::string match =
            "type='signal',interface='org.freedesktop.portal.Request',"
            "member='Response',path='" + requestPath + "'";
        dbus_bus_add_match(bus_, match.c_str(), &err);
        if (dbus_error_is_set(&err))
        {
            dbus_error_free(&err);
            return false;
        }

        waiters_[requestPath] = w;
        return true;
    }

    void unwatch(const std::string& requestPath)
    {
        if (!bus_ || requestPath.empty())
            return;

        waiters_.erase(requestPath);

        const std::string match =
            "type='signal',interface='org.freedesktop.portal.Request',"
            "member='Response',path='" + requestPath + "'";
        dbus_bus_remove_match(bus_, match.c_str(), nullptr);
    }

    // Ask the portal to take an outstanding request down (used when our window
    // goes away with a dialog still open). The portal still answers.
    void closeRequest(const std::string& requestPath)
    {
        if (!bus_ || requestPath.empty())
            return;

        DBusMessage* msg = dbus_message_new_method_call(
            "org.freedesktop.portal.Desktop", requestPath.c_str(),
            "org.freedesktop.portal.Request", "Close");
        if (!msg)
            return;

        dbus_connection_send(bus_, msg, nullptr);
        dbus_connection_flush(bus_);
        dbus_message_unref(msg);
    }

    // Non-blocking; called from the same poll loop as wayland and the timer.
    void pump()
    {
        if (!bus_)
            return;

        dbus_connection_read_write(bus_, 0);

        while (DBusMessage* msg = dbus_connection_pop_message(bus_))
        {
            dispatch(msg);
            dbus_message_unref(msg);
        }
    }

    // --- helpers shared with the dialog ---
    static void appendStringOption(DBusMessageIter* dict, const char* key, const char* value)
    {
        DBusMessageIter entry, variant;
        dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &value);
        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(dict, &entry);
    }

    // "file:///home/x/My%20Song.se" -> "/home/x/My Song.se". Anything that is not
    // a local file (a portal document, an ftp URI) has no path we can hand to
    // fopen, so it comes back empty rather than as something that looks usable.
    static std::string uriToPath(const std::string& uri)
    {
        constexpr const char* kFile = "file://";
        if (uri.rfind(kFile, 0) != 0)
            return {};

        std::string out;
        for (size_t i = strlen(kFile); i < uri.size(); ++i)
        {
            if (uri[i] == '%' && i + 2 < uri.size())
            {
                const auto hex = [](char c) -> int
                {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return -1;
                };
                const int hi = hex(uri[i + 1]), lo = hex(uri[i + 2]);
                if (hi >= 0 && lo >= 0)
                {
                    out += char(hi * 16 + lo);
                    i += 2;
                    continue;
                }
            }
            out += uri[i];
        }
        return out;
    }

    static std::vector<std::string> extractUris(DBusMessageIter* results)
    {
        std::vector<std::string> uris;

        DBusMessageIter dict;
        dbus_message_iter_recurse(results, &dict);

        while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY)
        {
            DBusMessageIter entry;
            dbus_message_iter_recurse(&dict, &entry);

            const char* key{};
            if (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING)
                dbus_message_iter_get_basic(&entry, &key);

            dbus_message_iter_next(&entry);

            if (key && 0 == strcmp(key, "uris") &&
                dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_VARIANT)
            {
                DBusMessageIter variant, array;
                dbus_message_iter_recurse(&entry, &variant);
                if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_ARRAY)
                {
                    dbus_message_iter_recurse(&variant, &array);
                    while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRING)
                    {
                        const char* uri{};
                        dbus_message_iter_get_basic(&array, &uri);
                        if (uri) uris.emplace_back(uri);
                        dbus_message_iter_next(&array);
                    }
                }
            }

            dbus_message_iter_next(&dict);
        }

        return uris;
    }

private:
    void dispatch(DBusMessage* msg)
    {
        if (!dbus_message_is_signal(msg, "org.freedesktop.portal.Request", "Response"))
            return;

        const char* path = dbus_message_get_path(msg);
        if (!path)
            return;

        auto it = waiters_.find(path);
        if (it == waiters_.end())
            return;

        DBusMessageIter args;
        dbus_message_iter_init(msg, &args);

        uint32_t response = 1;   // 0 = chosen, 1 = cancelled, 2 = ended otherwise
        if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_UINT32)
            dbus_message_iter_get_basic(&args, &response);
        dbus_message_iter_next(&args);

        std::vector<std::string> uris;
        if (response == 0 && dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_ARRAY)
            uris = extractUris(&args);

        // Take the waiter out first: the callback commonly destroys the dialog.
        auto* w = it->second;
        waiters_.erase(it);
        w->onPortalResponse(response, uris);
    }

    DBusConnection* bus_{};
    std::map<std::string, Waiter*> waiters_;
    int counter_ = 0;
};

// ---------------------------------------------------------------------------
// PortalFileDialog - IFileDialog
// ---------------------------------------------------------------------------
class PortalFileDialog : public gmpi::api::IFileDialog, private PortalBus::Waiter
{
public:
    PortalFileDialog(PortalBus& bus, int32_t dialogType, std::string parentWindow)
        : bus_(bus), save_(dialogType == 1), parentWindow_(std::move(parentWindow)) {}

    ~PortalFileDialog()
    {
        if (!requestPath_.empty())
        {
            // going away with a dialog still up: take it down rather than leave
            // the portal talking to nobody
            bus_.closeRequest(requestPath_);
            bus_.unwatch(requestPath_);
        }
    }

    gmpi::ReturnCode addExtension(const char* extension, const char* description) override
    {
        if (extension && *extension)
            filters_.push_back({ description && *description ? description : extension, extension });
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode setInitialFilename(const char* text) override
    {
        initialName_ = text ? text : "";
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode setInitialDirectory(const char* text) override
    {
        initialDir_ = text ? text : "";
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode showAsync(const gmpi::drawing::Rect*, gmpi::api::IUnknown* callback) override;

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IFileDialog);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

private:
    void onPortalResponse(uint32_t response, const std::vector<std::string>& uris) override
    {
        requestPath_.clear();

        std::string path;
        if (response == 0 && !uris.empty())
            path = PortalBus::uriToPath(uris.front());

        // Cancelled, or chose something with no local path: report failure and an
        // empty name, never a half-usable one.
        const auto result = path.empty() ? gmpi::ReturnCode::Cancel : gmpi::ReturnCode::Ok;

        if (auto cb = callback_.as<gmpi::api::IFileDialogCallback>(); cb)
            cb->onComplete(result, path.c_str());

        callback_ = {};
        release();   // balances the addRef in showAsync
    }

    void appendFilters(DBusMessageIter* options) const;

    PortalBus&  bus_;
    bool        save_{};
    std::string parentWindow_;
    std::string initialName_;
    std::string initialDir_;
    std::string requestPath_;

    struct Filter { std::string description, extension; };
    std::vector<Filter> filters_;

    gmpi::shared_ptr<gmpi::api::IUnknown> callback_;
};

// filters: a(sa(us)) - a list of (name, list of (type, pattern)) where type 0 is
// a glob and 1 a MIME type. We only ever have globs.
inline void PortalFileDialog::appendFilters(DBusMessageIter* options) const
{
    if (filters_.empty())
        return;

    DBusMessageIter entry, variant, list;
    const char* key = "filters";

    dbus_message_iter_open_container(options, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "a(sa(us))", &variant);
    dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "(sa(us))", &list);

    for (const auto& f : filters_)
    {
        DBusMessageIter filter, patterns, pattern;
        dbus_message_iter_open_container(&list, DBUS_TYPE_STRUCT, nullptr, &filter);

        const std::string label = f.description + " (*." + f.extension + ")";
        const char* labelPtr = label.c_str();
        dbus_message_iter_append_basic(&filter, DBUS_TYPE_STRING, &labelPtr);

        dbus_message_iter_open_container(&filter, DBUS_TYPE_ARRAY, "(us)", &patterns);
        dbus_message_iter_open_container(&patterns, DBUS_TYPE_STRUCT, nullptr, &pattern);

        const uint32_t kGlob = 0;
        const std::string glob = "*." + f.extension;
        const char* globPtr = glob.c_str();
        dbus_message_iter_append_basic(&pattern, DBUS_TYPE_UINT32, &kGlob);
        dbus_message_iter_append_basic(&pattern, DBUS_TYPE_STRING, &globPtr);

        dbus_message_iter_close_container(&patterns, &pattern);
        dbus_message_iter_close_container(&filter, &patterns);
        dbus_message_iter_close_container(&list, &filter);
    }

    dbus_message_iter_close_container(&variant, &list);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(options, &entry);
}

inline gmpi::ReturnCode PortalFileDialog::showAsync(const gmpi::drawing::Rect*,
                                                    gmpi::api::IUnknown* callback)
{
    if (!bus_.connect())
        return gmpi::ReturnCode::Fail;

    const std::string token = bus_.nextToken();
    requestPath_ = bus_.predictPath(token);

    // watch BEFORE sending, or the response can arrive before we are listening
    if (!bus_.watch(requestPath_, this))
    {
        requestPath_.clear();
        return gmpi::ReturnCode::Fail;
    }

    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.FileChooser",
        save_ ? "SaveFile" : "OpenFile");

    if (!msg)
    {
        bus_.unwatch(requestPath_);
        requestPath_.clear();
        return gmpi::ReturnCode::Fail;
    }

    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);

    // parent_window: "wayland:<xdg_foreign handle>" so the portal can parent and
    // modally block the right window. Empty is legal but leaves it unparented.
    const char* parent = parentWindow_.c_str();
    const char* title  = save_ ? "Save File" : "Open File";
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parent);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &title);

    DBusMessageIter options;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &options);
    PortalBus::appendStringOption(&options, "handle_token", token.c_str());
    if (!initialName_.empty())
        PortalBus::appendStringOption(&options, "current_name", initialName_.c_str());
    if (!initialDir_.empty())
        PortalBus::appendStringOption(&options, "current_folder", initialDir_.c_str());
    appendFilters(&options);
    dbus_message_iter_close_container(&args, &options);

    const bool sent = dbus_connection_send(bus_.connection(), msg, nullptr);
    dbus_connection_flush(bus_.connection());
    dbus_message_unref(msg);

    if (!sent)
    {
        bus_.unwatch(requestPath_);
        requestPath_.clear();
        return gmpi::ReturnCode::Fail;
    }

    callback_ = callback;

    // Stay alive until the portal answers, independent of the caller's reference -
    // same contract as the popup menu.
    addRef();
    return gmpi::ReturnCode::Ok;
}

} // namespace wayland
} // namespace gmpi
