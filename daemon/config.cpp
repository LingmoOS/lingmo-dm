#include "config.h"

#include "base/settings.h"
#include "base/strings.h"

#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <string_view>
#include <system_error>
#include <vector>

using namespace Lingmo;

namespace dm {

namespace {

// Reads one configuration file over the values already collected. The first
// boot OOBE and the live image still write sddm-style files, so the same keys
// are accepted from there.
void applySettings(const std::string &path, Config &config)
{
    if (access(path.c_str(), F_OK) != 0)
        return;

    const base::Settings settings(path);

    config.greeterVt = int(settings.intValue("Greeter/Vt", config.greeterVt));
    config.greeterUser = settings.stringValue("Greeter/User", config.greeterUser);
    if (settings.contains("Greeter/Command"))
        config.greeterCommand = splitCommand(settings.stringValue("Greeter/Command"));
    config.greeterService = settings.stringValue("Greeter/Service", config.greeterService);

    config.sessionVt = int(settings.intValue("Session/Vt", config.sessionVt));
    config.sessionService = settings.stringValue("Session/Service", config.sessionService);
    if (settings.contains("Session/Directories"))
        config.sessionDirectories = base::strings::split(settings.stringValue("Session/Directories"), ':',
                                                         base::strings::SplitBehavior::SkipEmptyParts);
    config.defaultSession = settings.stringValue("Session/Default", config.defaultSession);

    config.autologinUser = settings.stringValue("Autologin/User", config.autologinUser);
    config.autologinSession = settings.stringValue("Autologin/Session", config.autologinSession);

    config.runtimeDirectory = settings.stringValue("Daemon/RuntimeDirectory", config.runtimeDirectory);
    config.statePath = settings.stringValue("Daemon/StatePath", config.statePath);
}

} // namespace

Config Config::load(const std::string &path)
{
    Config config;

    // sddm compatibility, lowest priority first: the distribution's own file
    // last. This is what the OOBE (oobe.sh) and the live image write.
    applySettings("/etc/sddm.conf", config);

    std::error_code error;
    std::vector<std::string> compatibilityFiles;
    for (const auto &entry : std::filesystem::directory_iterator(
                 "/etc/sddm.conf.d", std::filesystem::directory_options::skip_permission_denied, error)) {
        if (error)
            break;
        if (entry.is_regular_file() && entry.path().extension() == ".conf")
            compatibilityFiles.push_back(entry.path().string());
    }
    std::sort(compatibilityFiles.begin(), compatibilityFiles.end());
    for (const auto &file : compatibilityFiles)
        applySettings(file, config);

    applySettings(path, config);

    // sddm writes the session with its desktop file suffix; the sessions here
    // are named by the file name only, e.g. "lingmo-oobe".
    static constexpr std::string_view desktopSuffix = ".desktop";
    if (base::strings::endsWith(config.autologinSession, desktopSuffix))
        config.autologinSession.resize(config.autologinSession.size() - desktopSuffix.size());

    return config;
}

std::vector<std::string> splitCommand(const std::string &command)
{
    std::vector<std::string> arguments;
    std::string argument;
    int quotes = 0;
    bool inQuote = false;

    for (const char c : command) {
        if (c == '"') {
            // The third quote in a row is a literal one.
            if (++quotes == 3) {
                quotes = 0;
                argument += c;
            }
            continue;
        }
        if (quotes) {
            if (quotes == 1)
                inQuote = !inQuote;
            quotes = 0;
        }
        if (!inQuote && base::strings::isAsciiWhitespace(c)) {
            if (!argument.empty()) {
                arguments.push_back(argument);
                argument.clear();
            }
        } else {
            argument += c;
        }
    }
    if (!argument.empty())
        arguments.push_back(argument);

    return arguments;
}

} // namespace dm
