#include "librarytestbase.h"

namespace {

// Every file under a folder with its bytes, or just the one file; for comparing before and after.
QMap<QString, QByteArray> snapshotOf(const QString &path)
{
    QMap<QString, QByteArray> out;
    if (QFileInfo(path).isFile()) {
        out.insert(path, readFile(path));
        return out;
    }
    QDirIterator it(path, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        out.insert(file, readFile(file));
    }
    return out;
}

} // namespace

// Undo: the last change put back byte for byte (see LibraryUndo).
class LibraryUndoTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void editSave();
    void editSaveWithArt();
    void unchangedSaveOffersNothing();
    void hideOffersUndo();
    void romChangeHasNoUndo();
    void deleteGameFileOnly();
    void deleteIndexedGame();
    void deleteTagPutsItBack();
    void movePlatform();
    void newerChangeReplacesOlder();
    void startupRemovesLeftovers();
    void expires();
};

namespace {

quint64 lastToken(QSignalSpy &spy)
{
    return spy.isEmpty() ? 0 : spy.last().at(0).value<quint64>();
}

} // namespace

void LibraryUndoTest::editSave()
{
    auto library = openLibrary();
    const QString file = m_data + QStringLiteral("/library/games/switch-pikmin-4.toml");
    const QByteArray before = readFile(file);
    QSignalSpy offered(library.get(), &Library::undoOffered);

    QCOMPARE(library->saveGame("switch-pikmin-4", {{"description", "Changed"}, {"rating", 4}}), QString());
    QVERIFY(readFile(file) != before);
    QCOMPARE(offered.count(), 1);
    QVERIFY(offered.last().at(1).toString().contains("Pikmin 4"));

    QCOMPARE(library->undo(lastToken(offered)), QString());
    QCOMPARE(readFile(file), before);
    QTRY_COMPARE(library->gameForEditing("switch-pikmin-4").value("rating").toDouble(), -1.0);
    // Once.
    QVERIFY(!library->undo(lastToken(offered)).isEmpty());
}

void LibraryUndoTest::editSaveWithArt()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-pikmin-4");
    const QString file = m_data + QStringLiteral("/library/games/") + id + QStringLiteral(".toml");
    const QString art = m_data + QStringLiteral("/art/") + id;
    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::red);
    const QString first = m_dir->filePath(QStringLiteral("first.png"));
    const QString second = m_dir->filePath(QStringLiteral("second.png"));
    QVERIFY(image.save(first));
    image.fill(Qt::blue);
    QVERIFY(image.save(second));
    QCOMPARE(library->setArt(id, "cover", first), QString());
    const QByteArray before = readFile(file);
    const auto artBefore = snapshotOf(art);
    QVERIFY(!artBefore.isEmpty());
    QSignalSpy offered(library.get(), &Library::undoOffered);

    // The editor: start, change the art, save.
    library->beginEditUndo(id);
    QCOMPARE(library->setArt(id, "cover", second), QString());
    QCOMPARE(library->setArt(id, "logo", first), QString());
    QCOMPARE(library->saveGame(id, {{"description", "Changed"}}), QString());
    QVERIFY(snapshotOf(art) != artBefore);
    QCOMPARE(offered.count(), 1);

    QCOMPARE(library->undo(lastToken(offered)), QString());
    QCOMPARE(readFile(file), before);
    QCOMPARE(snapshotOf(art), artBefore);
    QTRY_VERIFY(!library->gameForEditing(id).value("art").toMap().value("cover").toString().isEmpty());
    QVERIFY(library->gameForEditing(id).value("art").toMap().value("logo").toString().isEmpty());
}

void LibraryUndoTest::unchangedSaveOffersNothing()
{
    auto library = openLibrary();
    QSignalSpy offered(library.get(), &Library::undoOffered);
    QCOMPARE(library->saveGame("switch-pikmin-4", {}), QString());
    library->beginEditUndo("switch-pikmin-4");
    QCOMPARE(library->saveGame("switch-pikmin-4", {}), QString());
    QCOMPARE(offered.count(), 0);
    QVERIFY(!library->undoLog().pending());
}

void LibraryUndoTest::hideOffersUndo()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-pikmin-4");
    QSignalSpy offered(library.get(), &Library::undoOffered);
    QCOMPARE(library->removeGame(id, false), QString());
    QCOMPARE(offered.count(), 1);
    QVERIFY(offered.last().at(1).toString().contains("hidden"));
    const QString file = m_data + QStringLiteral("/library/games/") + id + QStringLiteral(".toml");
    QVERIFY(readFile(file).contains("hidden = true"));

    QCOMPARE(library->undo(lastToken(offered)), QString());
    QVERIFY(!readFile(file).contains("hidden = true"));
}

void LibraryUndoTest::romChangeHasNoUndo()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-pikmin-4");
    const QString rom = m_installs.home + QStringLiteral("/Games/Pikmin.nsp");
    writeFile(rom, {});
    QSignalSpy offered(library.get(), &Library::undoOffered);
    library->beginEditUndo(id);
    QCOMPARE(library->saveGame(id, {{"rom", QUrl::fromLocalFile(rom).toString()}}), QString());
    QCOMPARE(offered.count(), 1);
    QCOMPARE(lastToken(offered), quint64(0));
    QVERIFY(offered.last().at(1).toString().contains("ROM"));
    QVERIFY(!library->undoLog().pending());
}

void LibraryUndoTest::deleteGameFileOnly()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("linux-supertuxkart");
    const QString file = m_data + QStringLiteral("/library/games/") + id + QStringLiteral(".toml");
    const QString history = m_data + QStringLiteral("/library/history/") + id + QStringLiteral(".toml");
    const QString art = m_data + QStringLiteral("/art/") + id;
    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::green);
    const QString png = m_dir->filePath(QStringLiteral("a.png"));
    QVERIFY(image.save(png));
    QCOMPARE(library->setArt(id, "icon", png), QString());
    writeFile(history, "[[session]]\nend = 2026-10-10T22:41:00+02:00\nminutes = 95\n");
    const QByteArray fileBefore = readFile(file);
    const QByteArray historyBefore = readFile(history);
    const auto artBefore = snapshotOf(art);
    QVERIFY(!fileBefore.isEmpty() && !artBefore.isEmpty());
    QSignalSpy offered(library.get(), &Library::undoOffered);

    QCOMPARE(library->removeGame(id, true), QString());
    QVERIFY(!QFileInfo::exists(file));
    QVERIFY(!QFileInfo::exists(history));
    QVERIFY(!QFileInfo::exists(art));
    QCOMPARE(library->rowOfGame(id), -1);
    QCOMPARE(offered.count(), 1);

    QCOMPARE(library->undo(lastToken(offered)), QString());
    QCOMPARE(readFile(file), fileBefore);
    QCOMPARE(readFile(history), historyBefore);
    QCOMPARE(snapshotOf(art), artBefore);
    QTRY_VERIFY(library->rowOfGame(id) >= 0);
}

void LibraryUndoTest::deleteIndexedGame()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-pikmin-4");
    const QString file = m_data + QStringLiteral("/library/games/") + id + QStringLiteral(".toml");
    const QByteArray before = readFile(file);
    QVERIFY(!before.isEmpty());
    QSignalSpy offered(library.get(), &Library::undoOffered);

    QCOMPARE(library->removeGame(id, true), QString());
    QVERIFY(readFile(file) != before);
    QCOMPARE(library->undo(lastToken(offered)), QString());
    QCOMPARE(readFile(file), before);
}

void LibraryUndoTest::deleteTagPutsItBack()
{
    auto library = openLibrary();
    const QString tags = m_data + QStringLiteral("/library/tags.toml");
    const QByteArray tagsBefore = readFile(tags);
    QMap<QString, QByteArray> gamesBefore;
    QDirIterator it(m_data + QStringLiteral("/library/games"), {"*.toml"}, QDir::Files);
    while (it.hasNext()) {
        const QString file = it.next();
        gamesBefore.insert(file, readFile(file));
    }
    library->setTagFilter({"Racing", "Adventure"});
    QSignalSpy offered(library.get(), &Library::undoOffered);

    QCOMPARE(library->removeTag("Racing"), QString());
    QVERIFY(readFile(tags) != tagsBefore);
    QVERIFY(!readFile(tags).contains("\"Racing\""));
    QCOMPARE(library->tagFilter(), QStringList({"Adventure"}));
    QVERIFY(offered.last().at(1).toString().contains("Racing"));

    QCOMPARE(library->undo(lastToken(offered)), QString());
    QCOMPARE(readFile(tags), tagsBefore); // in its place, byte for byte
    QDirIterator after(m_data + QStringLiteral("/library/games"), {"*.toml"}, QDir::Files);
    while (after.hasNext()) {
        const QString file = after.next();
        QCOMPARE(readFile(file), gamesBefore.value(file));
    }
    QCOMPARE(library->tagFilter(), QStringList({"Adventure", "Racing"}));
    QTRY_VERIFY(std::any_of(library->tags().cbegin(), library->tags().cend(),
                            [](const QVariant &t) { return t.toMap().value("name") == "Racing" && t.toMap().value("games").toInt() > 0; }));
}

void LibraryUndoTest::movePlatform()
{
    auto library = openLibrary();
    const QString file = m_data + QStringLiteral("/library/platforms.toml");
    const QByteArray before = readFile(file);
    QSignalSpy offered(library.get(), &Library::undoOffered);

    QCOMPARE(library->movePlatform("proton", -1), QString());
    QVERIFY(readFile(file) != before);
    QCOMPARE(library->undo(lastToken(offered)), QString());
    QCOMPARE(readFile(file), before);

    // Nothing moved, nothing to undo.
    offered.clear();
    QCOMPARE(library->movePlatform("switch", -1), QString());
    QCOMPARE(offered.count(), 0);
}

void LibraryUndoTest::newerChangeReplacesOlder()
{
    auto library = openLibrary();
    QSignalSpy offered(library.get(), &Library::undoOffered);
    const QString undoDir = m_state + QStringLiteral("/undo");

    QCOMPARE(library->removeGame("linux-supertuxkart", true), QString());
    const quint64 first = lastToken(offered);
    QVERIFY(QFileInfo::exists(undoDir + u'/' + QString::number(first)));
    QCOMPARE(library->movePlatform("proton", -1), QString());
    const quint64 second = lastToken(offered);
    QVERIFY(second != first);
    QVERIFY(!QFileInfo::exists(undoDir + u'/' + QString::number(first)));
    QCOMPARE(library->undo(first), QStringLiteral("Nothing to undo"));
    QVERIFY(!QFileInfo::exists(m_data + QStringLiteral("/library/games/linux-supertuxkart.toml")));
    QCOMPARE(library->undo(second), QString());
    QVERIFY(!QFileInfo::exists(undoDir + u'/' + QString::number(second)));
}

void LibraryUndoTest::startupRemovesLeftovers()
{
    const QString leftover = m_state + QStringLiteral("/undo/7/data/art/x/cover.png");
    writeFile(leftover, "old");
    QVERIFY(QFileInfo::exists(leftover));
    auto library = openLibrary();
    QVERIFY(!QFileInfo::exists(m_state + QStringLiteral("/undo/7")));
}

void LibraryUndoTest::expires()
{
    auto library = openLibrary();
    library->undoLog().setLifetime(20);
    QSignalSpy expired(library.get(), &Library::undoExpired);
    QSignalSpy offered(library.get(), &Library::undoOffered);
    QCOMPARE(library->removeGame("linux-supertuxkart", true), QString());
    const quint64 token = lastToken(offered);
    QTRY_COMPARE(expired.count(), 1);
    QCOMPARE(library->undo(token), QStringLiteral("Nothing to undo"));
    QVERIFY(!QFileInfo::exists(m_state + QStringLiteral("/undo/") + QString::number(token)));
}

QTEST_GUILESS_MAIN(LibraryUndoTest)
#include "library_undo_test.moc"
