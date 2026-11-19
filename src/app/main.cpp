// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QImageReader>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include "controller.h"
#include "imagecache.h"
#include "imageurl.h"
#include "seedpick_version.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("seedpick");

    // Breeze via qqc2-desktop-style when available, Kirigami's fallback otherwise.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    KAboutData about(QStringLiteral("seedpick"),
                     i18nc("@title", "Seedpick"),
                     QStringLiteral(SEEDPICK_VERSION_STRING),
                     i18n("Pick the best images from a folder by pairwise comparison"),
                     KAboutLicense::GPL_V2,
                     i18n("© 2026 Christian Charon"));
    about.addAuthor(i18n("Christian Charon"), QString(), QStringLiteral("ccharon@mailbox.org"));
    about.setDesktopFileName(QStringLiteral(SEEDPICK_APP_ID));
    about.setBugAddress("https://github.com/ccharon/seedpick/issues");
    about.setHomepage(QStringLiteral("https://github.com/ccharon/seedpick"));
    KAboutData::setApplicationData(about);
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral(SEEDPICK_APP_ID)));

    QCommandLineParser parser;
    parser.addPositionalArgument(QStringLiteral("directory"), i18n("Image directory to open"), QStringLiteral("[directory]"));
    about.setupCommandLine(&parser);
    parser.process(app);
    about.processCommandLine(&parser);

    QStringList extensions;
    for (const QByteArray &format : QImageReader::supportedImageFormats()) {
        extensions.append(QString::fromLatin1(format).toLower());
    }
    Controller controller(extensions);
    Controller::setInstance(&controller);
    if (!parser.positionalArguments().isEmpty()) {
        controller.openPath(parser.positionalArguments().constFirst());
    }

    ImageCache imageCache(qint64(256) * 1024 * 1024);
    ImageCache::setInstance(&imageCache);

    QQmlApplicationEngine engine;
    engine.addImageProvider(ImageUrl::providerId, new CachedImageProvider(&imageCache));
    KLocalization::setupLocalizedContext(&engine);
    engine.loadFromModule("io.github.ccharon.seedpick", "Main");

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
