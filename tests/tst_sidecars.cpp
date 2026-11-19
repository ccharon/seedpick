// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QTest>

#include "sidecars.h"

using namespace Seedpick;
using namespace Qt::Literals::StringLiterals;

class TestSidecars : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void baseName_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("base");
        QTest::newRow("simple") << u"img.png"_s << u"img"_s;
        QTest::newRow("several dots") << u"a.b.png"_s << u"a.b"_s;
        QTest::newRow("no dot") << u"img"_s << u"img"_s;
    }

    void baseName()
    {
        QFETCH(QString, file);
        QFETCH(QString, base);
        QCOMPARE(Seedpick::baseName(file), base);
    }

    void findsBothNamingSchemes()
    {
        const QStringList entries{u"img.png"_s, u"img.txt"_s, u"img.png.xmp"_s, u"img.png.json"_s, u"other.txt"_s};
        const auto result = findSidecars({u"img.png"_s}, entries, defaultSidecarExtensions());
        QCOMPARE(result.value(u"img.png"_s), (QStringList{u"img.png.json"_s, u"img.png.xmp"_s, u"img.txt"_s}));
        QCOMPARE(result.size(), 1);
    }

    void extensionsAreCaseInsensitive()
    {
        const auto result = findSidecars({u"img.png"_s}, {u"img.TXT"_s, u"img.Caption"_s}, {u"txt"_s, u"CAPTION"_s});
        QCOMPARE(result.value(u"img.png"_s), (QStringList{u"img.Caption"_s, u"img.TXT"_s}));
    }

    void ignoresOtherExtensions()
    {
        const auto result = findSidecars({u"img.png"_s}, {u"img.psd"_s, u"img.png.bak"_s}, defaultSidecarExtensions());
        QVERIFY(result.isEmpty());
    }

    void sharedBaseBelongsToNobody()
    {
        const QStringList images{u"img.png"_s, u"img.jpg"_s};
        const auto result = findSidecars(images, {u"img.txt"_s, u"img.png.xmp"_s}, defaultSidecarExtensions());
        QCOMPARE(result.value(u"img.png"_s), QStringList{u"img.png.xmp"_s});
        QVERIFY(!result.contains(u"img.jpg"_s));
    }

    void imagesAreNeverSidecars()
    {
        // A .json image is unusual but must not be moved as a sidecar of img.png.
        const QStringList images{u"img.png"_s, u"img.json"_s};
        const auto result = findSidecars(images, images, defaultSidecarExtensions());
        QVERIFY(result.isEmpty());
    }

    void namesWithSeveralDots()
    {
        const auto result = findSidecars({u"a.b.png"_s}, {u"a.b.txt"_s, u"a.txt"_s}, defaultSidecarExtensions());
        QCOMPARE(result.value(u"a.b.png"_s), QStringList{u"a.b.txt"_s});
    }
};

QTEST_GUILESS_MAIN(TestSidecars)
#include "tst_sidecars.moc"
