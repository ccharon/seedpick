// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include "imagescanner.h"

using namespace Seedpick;

namespace
{
void touch(const QString &path, const QByteArray &content = QByteArrayLiteral("x"))
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    file.write(content);
}

QStringList fileNames(const QList<ImageInfo> &images)
{
    QStringList names;
    for (const ImageInfo &image : images) {
        names.append(image.file);
    }
    return names;
}

const QStringList extensions{QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("webp")};
}

class TestImageScanner : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void filtersByExtensionCaseInsensitive()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        touch(dir.filePath(QStringLiteral("a.png")));
        touch(dir.filePath(QStringLiteral("b.JPG")));
        touch(dir.filePath(QStringLiteral("a.txt")));
        touch(dir.filePath(QStringLiteral("noext")));

        const ScanResult result = scanImages(dir.path(), extensions);
        QVERIFY(result.error.isEmpty());
        QCOMPARE(fileNames(result.images), (QStringList{QStringLiteral("a.png"), QStringLiteral("b.JPG")}));
    }

    void skipsSubdirectoriesAndHiddenFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QDir root(dir.path());
        QVERIFY(root.mkdir(QStringLiteral("_keep")));
        QVERIFY(root.mkdir(QStringLiteral("folder.png")));
        touch(dir.filePath(QStringLiteral("_keep/inner.png")));
        touch(dir.filePath(QStringLiteral(".hidden.png")));
        touch(dir.filePath(QStringLiteral(".seedpick-session.json")));
        touch(dir.filePath(QStringLiteral("visible.png")));

        const ScanResult result = scanImages(dir.path(), extensions);
        QCOMPARE(fileNames(result.images), QStringList{QStringLiteral("visible.png")});
    }

    void sortsNaturally()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const char *name : {"img10.png", "img2.png", "Img1.png", "img1.png", "img002.png", "b.png"}) {
            touch(dir.filePath(QString::fromLatin1(name)));
        }

        const ScanResult result = scanImages(dir.path(), extensions);
        QCOMPARE(fileNames(result.images),
                 (QStringList{QStringLiteral("b.png"),
                              QStringLiteral("Img1.png"),
                              QStringLiteral("img1.png"),
                              QStringLiteral("img002.png"),
                              QStringLiteral("img2.png"),
                              QStringLiteral("img10.png")}));
    }

    void reportsSizeAndMtime()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("a.png"));
        touch(path, QByteArray(1234, 'x'));
        const QDateTime mtime(QDate(2025, 3, 4), QTime(5, 6, 7, 890), QTimeZone::UTC);
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadWrite));
            QVERIFY(file.setFileTime(mtime, QFileDevice::FileModificationTime));
        }

        const ScanResult result = scanImages(dir.path(), extensions);
        QCOMPARE(result.images.size(), 1);
        QCOMPARE(result.images.first().size, 1234);
        QCOMPARE(result.images.first().mtime, mtime);
        QCOMPARE(result.images.first().mtime.timeSpec(), Qt::UTC);
    }

    void emptyDirectory()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const ScanResult result = scanImages(dir.path(), extensions);
        QVERIFY(result.error.isEmpty());
        QVERIFY(result.images.isEmpty());
    }

    void missingDirectoryIsError()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const ScanResult result = scanImages(dir.filePath(QStringLiteral("missing")), extensions);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.images.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestImageScanner)
#include "tst_imagescanner.moc"
