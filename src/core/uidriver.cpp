#include "uidriver.h"

#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMetaEnum>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTextStream>

namespace {
constexpr int ItemTimeoutMs = 2000;
constexpr int ScriptTimeoutMs = 60000;
constexpr int PollMs = 50;
// Pages slide and fade; a click right after the last one would land on the page that is leaving.
constexpr int SettleMs = 250;

bool shownOnScreen(const QQuickItem *item)
{
    if (!item->isVisible() || item->width() <= 0 || item->height() <= 0)
        return false;
    for (const QQuickItem *at = item; at; at = at->parentItem()) {
        if (at->opacity() <= 0.05)
            return false;
    }
    return true;
}

void collect(QQuickItem *item, const std::function<void(QQuickItem *)> &visit)
{
    visit(item);
    for (QQuickItem *child : item->childItems())
        collect(child, visit);
}
}

UiDriver::UiDriver(QQuickWindow *window, const QString &scriptPath, QObject *parent)
    : QObject(parent), m_window(window)
{
    QFile file(scriptPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qInfo().noquote() << "uitest: FAIL can't read" << scriptPath;
        QTimer::singleShot(0, this, [this] { finish(1); });
        return;
    }
    QTextStream in(&file);
    for (int number = 1; !in.atEnd(); ++number) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(u'#'))
            continue;
        const qsizetype space = line.indexOf(u' ');
        m_commands.append({number, space < 0 ? line : line.first(space),
                           space < 0 ? QString() : line.sliced(space + 1).trimmed()});
    }
    m_poll.setInterval(PollMs);
    connect(&m_poll, &QTimer::timeout, this, &UiDriver::step);
}

void UiDriver::start()
{
    if (m_started)
        return;
    m_started = true;
    QTimer::singleShot(ScriptTimeoutMs, this, [this] { fail(QStringLiteral("the script ran past 60 s")); });
    m_waiting.start();
    m_poll.start();
}

void UiDriver::step()
{
    if (m_index >= m_commands.size()) {
        qInfo().noquote() << "uitest: done";
        finish(0);
        return;
    }
    const Command &command = m_commands.at(m_index);
    if (tryCommand(command)) {
        ++m_index;
        m_waiting.restart();
        return;
    }
    if (m_waiting.elapsed() > ItemTimeoutMs) {
        fail(QStringLiteral("line %1: %2 %3 didn't happen. On screen: %4")
                 .arg(command.line)
                 .arg(command.name, command.argument, visibleTexts().join(QStringLiteral(" | "))));
    }
}

bool UiDriver::tryCommand(const Command &command)
{
    // A command that waits for a state leaves the poll running; one that acts once pauses it so the page can settle.
    const auto settle = [this] {
        m_poll.stop();
        QTimer::singleShot(SettleMs, this, [this] { m_poll.start(); });
    };

    if (command.name == u"wait") {
        m_poll.stop();
        QTimer::singleShot(command.argument.toInt(), this, [this] { m_poll.start(); });
        return true;
    }
    if (command.name == u"click") {
        QQuickItem *item = find(command.argument);
        if (!item)
            return false;
        click(item);
        qInfo().noquote() << "uitest: click" << command.argument;
        settle();
        return true;
    }
    if (command.name == u"key") {
        if (!key(command.argument)) {
            fail(QStringLiteral("line %1: unknown key %2").arg(command.line).arg(command.argument));
            return false;
        }
        qInfo().noquote() << "uitest: key" << command.argument;
        settle();
        return true;
    }
    if (command.name == u"expect")
        return find(command.argument) != nullptr;
    if (command.name == u"expect-not")
        return find(command.argument) == nullptr;
    if (command.name == u"expect-has")
        return find(command.argument, true) != nullptr;
    if (command.name == u"expect-not-has")
        return find(command.argument, true) == nullptr;
    if (command.name == u"shot") {
        m_window->grabWindow().save(command.argument);
        return true;
    }
    if (command.name == u"quit") {
        qInfo().noquote() << "uitest: done";
        finish(0);
        return true;
    }
    fail(QStringLiteral("line %1: unknown command %2").arg(command.line).arg(command.name));
    return false;
}

QQuickItem *UiDriver::find(const QString &text, bool contains) const
{
    QQuickItem *found = nullptr;
    // The last match is the one drawn on top.
    collect(m_window->contentItem(), [&](QQuickItem *item) {
        const QVariant value = item->property("text");
        if (value.typeId() == QMetaType::QString
            && (contains ? value.toString().contains(text) : value.toString() == text) && shownOnScreen(item))
            found = item;
    });
    return found;
}

QStringList UiDriver::visibleTexts() const
{
    QStringList texts;
    collect(m_window->contentItem(), [&](QQuickItem *item) {
        const QVariant value = item->property("text");
        if (value.typeId() == QMetaType::QString && !value.toString().isEmpty() && shownOnScreen(item)
            && texts.size() < 60)
            texts.append(value.toString().left(40));
    });
    return texts;
}

void UiDriver::click(QQuickItem *item)
{
    const QPointF scene = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
    const QPointF global = m_window->mapToGlobal(scene.toPoint());
    const auto send = [&](QEvent::Type type, Qt::MouseButtons buttons) {
        QMouseEvent event(type, scene, scene, global,
                          type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(m_window, &event);
    };
    send(QEvent::MouseMove, Qt::NoButton);
    send(QEvent::MouseButtonPress, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, Qt::NoButton);
}

bool UiDriver::key(const QString &name)
{
    bool ok = false;
    const int value = QMetaEnum::fromType<Qt::Key>().keyToValue(("Key_" + name.toLatin1()).constData(), &ok);
    if (!ok)
        return false;
    QKeyEvent press(QEvent::KeyPress, value, Qt::NoModifier);
    QCoreApplication::sendEvent(m_window, &press);
    QKeyEvent release(QEvent::KeyRelease, value, Qt::NoModifier);
    QCoreApplication::sendEvent(m_window, &release);
    return true;
}

void UiDriver::fail(const QString &reason)
{
    qInfo().noquote() << "uitest: FAIL" << reason;
    finish(1);
}

void UiDriver::finish(int code)
{
    m_poll.stop();
    QCoreApplication::exit(code);
}
