#include "librarytestbase.h"

void LibraryTestBase::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_data = m_dir->filePath(QStringLiteral("data"));
    m_cache = m_dir->filePath(QStringLiteral("cache"));
    m_state = m_dir->filePath(QStringLiteral("state/lattis"));
    copyTree(QStringLiteral(DEMO_DATA_DIR), m_data);
    m_installs.home = m_dir->filePath(QStringLiteral("home"));
    m_installs.flatpakDirs = {m_dir->filePath(QStringLiteral("flatpak"))};
    QDir().mkpath(m_installs.flatpakDirs.first() + QStringLiteral("/io.github.ryubing.Ryujinx/current/active"));
    QDir().mkpath(m_installs.flatpakDirs.first() + QStringLiteral("/../overrides"));
    QDir().mkpath(m_installs.flatpakDirs.first() + QStringLiteral("/org.azahar_emu.Azahar/current/active"));
    // Allowed to read everywhere, like most emulator Flatpaks.
    writeFile(m_installs.flatpakDirs.first() + QStringLiteral("/io.github.ryubing.Ryujinx/current/active/metadata"),
              "[Context]\nfilesystems=host;\n");
    writeFile(m_installs.flatpakDirs.first() + QStringLiteral("/org.azahar_emu.Azahar/current/active/metadata"),
              "[Context]\nfilesystems=host;\n");
}

void LibraryTestBase::cleanup()
{
    m_dir.reset();
}

std::unique_ptr<Library> LibraryTestBase::openLibrary()
{
    auto library = std::make_unique<Library>(m_data, m_cache);
    library->store().setInstallLocations(m_installs);
    library->store().setSteamLocations({m_installs.home, {}});
    library->store().setStateDir(m_state);
    library->setUndoDir(m_state + QStringLiteral("/undo"));
    // refresh() signals once as the scan starts, and the result once more. The scan's thread can
    // finish before its result is posted, so one processEvents() after waitForIdle() may miss it.
    QSignalSpy scanning(&library->store(), &LibraryStore::scanningChanged);
    library->startRefresh();
    library->store().waitForIdle();
    if (!QTest::qWaitFor([&] { return scanning.count() >= 2 && !library->store().scanning(); }, 10000))
        qWarning("openLibrary: the first scan's result never arrived");
    return library;
}

QString LibraryTestBase::libraryId() const
{
    return parseLibraryInfo(readFile(m_data + QStringLiteral("/library/library.toml")), {}).value.id;
}

QString LibraryTestBase::installFakeSteam()
{
    const QString root = m_installs.home + QStringLiteral("/.local/share/Steam");
    copyTree(QStringLiteral(STEAM_DATA_DIR), root);
    QFile::remove(m_data + QStringLiteral("/settings.toml")); // the demo's [steam] path
    QDir().mkpath(m_installs.home + QStringLiteral("/.steam"));
    QFile::link(root, m_installs.home + QStringLiteral("/.steam/steam"));
    return root;
}

std::unique_ptr<QProcess> LibraryTestBase::startFakeSteam(const QString &root)
{
    const QString exe = root + QStringLiteral("/ubuntu12_32/steam");
    QDir().mkpath(QFileInfo(exe).absolutePath());
    QFile::copy(QStandardPaths::findExecutable(QStringLiteral("sleep")), exe);
    QFile::setPermissions(exe, QFile::permissions(exe) | QFile::ExeUser);
    auto steam = std::make_unique<QProcess>();
    steam->start(exe, {QStringLiteral("60")});
    steam->waitForStarted();
    writeFile(m_installs.home + QStringLiteral("/.steam/steam.pid"), QByteArray::number(steam->processId()));
    return steam;
}

Uninstall::Trash LibraryTestBase::fakeTrash(QStringList *taken, bool works)
{
    const QString trash = m_dir->filePath(QStringLiteral("trash"));
    return [trash, taken, works](const QString &path, QString *error) {
        if (!works) {
            *error = QStringLiteral("there's no trash here");
            return false;
        }
        QDir().mkpath(trash);
        if (!QDir().rename(path, trash + u'/' + QString::number(taken->size()))) {
            *error = QStringLiteral("could not move it");
            return false;
        }
        taken->append(path);
        return true;
    };
}

QString LibraryTestBase::addUninstallGame(Library &library, quint32 *appId)
{
    const QString exe = m_installs.home + QStringLiteral("/Games/Hollow Knight/hollow_knight.exe");
    writeFile(exe, "MZ");
    writeFile(m_installs.home + QStringLiteral("/Games/Hollow Knight/save.dat"), "saved");
    const QString pics = m_dir->filePath(QStringLiteral("pics"));
    QImage hero(64, 32, QImage::Format_RGB32);
    hero.fill(Qt::red);
    QDir().mkpath(pics);
    hero.save(pics + QStringLiteral("/hero.png"));
    const QVariantMap added = library.addGame(exe, QStringLiteral("proton"), QStringLiteral("Hollow Knight"),
                                              {{"proton", "GE-Proton10-4"}}, {{"hero", pics + QStringLiteral("/hero.png")}});
    const QString id = added.value("id").toString();
    *appId = quint32(library.store().snapshot().content.games.value(id).shortcut);
    writeFile(m_installs.home + QStringLiteral("/.local/share/Steam/steamapps/compatdata/%1/pfx/user.reg").arg(*appId),
              "settings");
    return id;
}
