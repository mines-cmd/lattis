#pragma once

#include <QByteArray>
#include <QtEndian>

// Writes binary VDF bytes by hand, field by field, without BinaryVdf's writer, so the tests
// can compare what it reads and writes against a file built another way.
class VdfBytes
{
public:
    VdfBytes &map(const QByteArray &key) { return field(0x00, key); }
    VdfBytes &end()
    {
        m_bytes.append('\x08');
        return *this;
    }
    VdfBytes &str(const QByteArray &key, const QByteArray &value)
    {
        field(0x01, key).m_bytes.append(value).append('\0');
        return *this;
    }
    VdfBytes &i32(const QByteArray &key, qint32 value) { return number(0x02, key, value); }
    VdfBytes &f32(const QByteArray &key, float value) { return number(0x03, key, value); }
    VdfBytes &u64(const QByteArray &key, quint64 value) { return number(0x07, key, value); }
    VdfBytes &i64(const QByteArray &key, qint64 value) { return number(0x0A, key, value); }
    VdfBytes &raw(const QByteArray &bytes)
    {
        m_bytes.append(bytes);
        return *this;
    }

    QByteArray bytes() const { return m_bytes; }

    // The fields of a shortcut Steam added itself, in its order.
    VdfBytes &steamMadeShortcut(const QByteArray &key, quint32 appId, const QByteArray &name, const QByteArray &exe,
                                const QByteArray &startDir, const QByteArray &launchOptions, qint32 lastPlayTime)
    {
        return map(key)
            .i32("appid", qint32(appId))
            .str("appname", name)
            .str("exe", exe)
            .str("StartDir", startDir)
            .str("icon", "")
            .str("ShortcutPath", "")
            .str("LaunchOptions", launchOptions)
            .i32("IsHidden", 0)
            .i32("AllowDesktopConfig", 1)
            .i32("AllowOverlay", 1)
            .i32("OpenVR", 0)
            .i32("Devkit", 0)
            .str("DevkitGameID", "")
            .i32("DevkitOverrideAppID", 0)
            .i32("LastPlayTime", lastPlayTime)
            .str("FlatpakAppID", "")
            .str("sortas", "");
    }

private:
    VdfBytes &field(char type, const QByteArray &key)
    {
        m_bytes.append(type).append(key).append('\0');
        return *this;
    }
    template<typename T>
    VdfBytes &number(char type, const QByteArray &key, T value)
    {
        char data[sizeof(T)];
        qToLittleEndian(value, data);
        field(type, key).m_bytes.append(data, sizeof(T));
        return *this;
    }

    QByteArray m_bytes;
};

// tests/data/steam/userdata/39734273/config/shortcuts.vdf: a game run with Proton, an
// emulator shortcut as Steam ROM Manager writes them (unquoted, fewer fields, hidden), and
// one with arguments inside `exe`.
inline QByteArray fakeTreeShortcuts()
{
    VdfBytes b;
    b.map("shortcuts");
    b.steamMadeShortcut("0", 2900000001u, "Cave Story", "\"/home/user/Games/Cave Story/Doukutsu.exe\"",
                        "/home/user/Games/Cave Story/", " mangohud %command%", 1788000000)
        .map("tags")
        .end()
        .end();
    b.map("1")
        .i32("appid", qint32(3100000002u))
        .str("appname", "Monster Hunter 4 Ultimate")
        .str("exe", "/home/user/AppImages/azahar.appimage")
        .str("StartDir", "/home/user/AppImages")
        .str("icon", "")
        .str("LaunchOptions", "\"/home/user/Games/3ds/Monster Hunter 4 Ultimate.3ds\"")
        .i32("IsHidden", 1)
        .i32("LastPlayTime", 0)
        .map("tags")
        .end()
        .end();
    b.steamMadeShortcut("2", 3300000003u, "Super Smash Bros. Ultimate",
                        "\"/home/user/AppImages/ryujinx.appimage\" --fullscreen \"/home/user/Games/switch/Smash.nsp\"",
                        "/home/user/AppImages", "", 1789947306)
        .map("tags")
        .str("0", "Switch")
        .end()
        .end();
    return b.end().end().bytes();
}
