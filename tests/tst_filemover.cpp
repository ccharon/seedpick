// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include "filemover.h"

using namespace Seedpick;
using namespace Qt::Literals::StringLiterals;

namespace
{
void writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(data), data.size());
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

class TestFileMover : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void moves_data()
    {
        QTest::addColumn<LocalFileMover::Method>("method");
        QTest::newRow("rename") << LocalFileMover::Method::Rename;
        QTest::newRow("copy") << LocalFileMover::Method::Copy;
    }

    void moves()
    {
        QFETCH(LocalFileMover::Method, method);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString source = dir.filePath(u"a.png"_s);
        const QString target = dir.filePath(u"_keep/sub/a.png"_s);
        const QByteArray data = QByteArray(3 * 1024 * 1024 + 17, 'x') + "end";
        writeFile(source, data);
        const QDateTime mtime(QDate(2025, 3, 4), QTime(5, 6, 7), QTimeZone::UTC);
        {
            QFile file(source);
            QVERIFY(file.open(QIODevice::ReadWrite));
            QVERIFY(file.setFileTime(mtime, QFileDevice::FileModificationTime));
        }

        LocalFileMover mover(method);
        QString error;
        QVERIFY2(mover.move(source, target, &error), qPrintable(error));
        QVERIFY(!QFileInfo::exists(source));
        QCOMPARE(readFile(target), data);
        QCOMPARE(QFileInfo(target).lastModified().toUTC(), mtime);
    }

    void neverOverwrites_data()
    {
        moves_data();
    }

    void neverOverwrites()
    {
        QFETCH(LocalFileMover::Method, method);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString source = dir.filePath(u"a.png"_s);
        const QString target = dir.filePath(u"b.png"_s);
        writeFile(source, "source");
        writeFile(target, "target");

        QString error;
        QVERIFY(!LocalFileMover(method).move(source, target, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(readFile(source), QByteArray("source"));
        QCOMPARE(readFile(target), QByteArray("target"));
    }

    void refusesDanglingSymlinkTarget()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString source = dir.filePath(u"a.png"_s);
        const QString target = dir.filePath(u"link.png"_s);
        writeFile(source, "source");
        QVERIFY(QFile::link(dir.filePath(u"missing"_s), target));

        QVERIFY(!LocalFileMover().move(source, target, nullptr));
        QCOMPARE(readFile(source), QByteArray("source"));
        QVERIFY(!QFileInfo::exists(dir.filePath(u"missing"_s)));
    }

    void failsForMissingSource()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString error;
        QVERIFY(!LocalFileMover().move(dir.filePath(u"none.png"_s), dir.filePath(u"x/none.png"_s), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFileInfo::exists(dir.filePath(u"x"_s)));
    }

    void refusesDirectories()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(u"folder"_s));
        QVERIFY(!LocalFileMover().move(dir.filePath(u"folder"_s), dir.filePath(u"moved"_s), nullptr));
        QVERIFY(QFileInfo(dir.filePath(u"folder"_s)).isDir());
    }

    void failsWhenTargetFolderIsAFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        writeFile(dir.filePath(u"a.png"_s), "a");
        writeFile(dir.filePath(u"_keep"_s), "not a folder");
        QString error;
        QVERIFY(!LocalFileMover().move(dir.filePath(u"a.png"_s), dir.filePath(u"_keep/a.png"_s), &error));
        QCOMPARE(readFile(dir.filePath(u"a.png"_s)), QByteArray("a"));
    }
};

QTEST_GUILESS_MAIN(TestFileMover)
#include "tst_filemover.moc"
