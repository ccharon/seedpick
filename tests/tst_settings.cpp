// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <KLocalizedString>

#include <QCommandLineParser>
#include <QTest>

#include "settings.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
std::optional<CommandLineOverrides> parse(const QStringList &arguments, QString *error)
{
    QCommandLineParser parser;
    addCommandLineOptions(parser);
    if (!parser.parse(QStringList{u"seedpick"_s} + arguments)) {
        *error = parser.errorText();
        return std::nullopt;
    }
    return commandLineOverrides(parser, error);
}
}

class TestSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("seedpick");
    }

    void noOptionsChangeNothing()
    {
        QString error;
        const auto overrides = parse({}, &error);
        QVERIFY(overrides);
        QCOMPARE(overrides->options(), QStringList{});
        QCOMPARE(applyOverrides(Settings{}, *overrides), Settings{});
    }

    void optionsOverrideSettings()
    {
        QString error;
        const auto overrides = parse({u"-n"_s,
                                      u"8"_s,
                                      u"--keep-dir"_s,
                                      u"_auswahl"_s,
                                      u"--rest-dir=_rest2"_s,
                                      u"--reject-dir"_s,
                                      u"_ausschuss"_s,
                                      u"--reject-to-trash"_s,
                                      u"--skip-screening"_s},
                                     &error);
        QVERIFY2(overrides, qPrintable(error));
        QCOMPARE(overrides->options(),
                 (QStringList{u"--top"_s, u"--keep-dir"_s, u"--rest-dir"_s, u"--reject-dir"_s, u"--reject-to-trash"_s, u"--skip-screening"_s}));

        Settings stored;
        stored.minDuels = 3;
        const Settings effective = applyOverrides(stored, *overrides);
        QCOMPARE(effective.topN, 8);
        QCOMPARE(effective.folders.keep, u"_auswahl"_s);
        QCOMPARE(effective.folders.rest, u"_rest2"_s);
        QCOMPARE(effective.folders.reject, u"_ausschuss"_s);
        QVERIFY(effective.rejectToTrash);
        QVERIFY(effective.skipScreening);
        QCOMPARE(effective.minDuels, 3);
    }

    void emptyKeepFolderIsAllowed()
    {
        QString error;
        const auto overrides = parse({u"--keep-dir="_s}, &error);
        QVERIFY2(overrides, qPrintable(error));
        Settings stored;
        stored.folders.keep = u"_keep"_s;
        QVERIFY(applyOverrides(stored, *overrides).folders.keep.isEmpty());
        QCOMPARE(sanitized(stored).folders.keep, u"_keep"_s);
        QVERIFY(Settings{}.folders.keep.isEmpty());
    }

    void flagsDoNotSwitchStoredValuesOff()
    {
        Settings stored;
        stored.rejectToTrash = true;
        stored.skipScreening = true;
        const Settings effective = applyOverrides(stored, CommandLineOverrides{});
        QVERIFY(effective.rejectToTrash);
        QVERIFY(effective.skipScreening);
    }

    void invalidValuesAreRejected_data()
    {
        QTest::addColumn<QStringList>("arguments");
        QTest::newRow("top zero") << QStringList{u"--top"_s, u"0"_s};
        QTest::newRow("top text") << QStringList{u"-n"_s, u"five"_s};
        QTest::newRow("top too large") << QStringList{u"-n"_s, u"10000"_s};
        QTest::newRow("folder with slash") << QStringList{u"--keep-dir"_s, u"a/b"_s};
        QTest::newRow("parent folder") << QStringList{u"--rest-dir"_s, u".."_s};
        QTest::newRow("empty folder") << QStringList{u"--reject-dir="_s};
    }

    void invalidValuesAreRejected()
    {
        QFETCH(QStringList, arguments);
        QString error;
        QVERIFY(!parse(arguments, &error));
        QVERIFY(!error.isEmpty());
    }

    void folderNames_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("valid");
        QTest::newRow("default") << u"_keep"_s << true;
        QTest::newRow("spaces inside") << u"my picks"_s << true;
        QTest::newRow("hidden") << u".keep"_s << true;
        QTest::newRow("empty") << QString() << false;
        QTest::newRow("blank") << u"  "_s << false;
        QTest::newRow("leading space") << u" _keep"_s << false;
        QTest::newRow("slash") << u"a/b"_s << false;
        QTest::newRow("dot") << u"."_s << false;
        QTest::newRow("dot dot") << u".."_s << false;
    }

    void folderNames()
    {
        QFETCH(QString, name);
        QFETCH(bool, valid);
        QCOMPARE(isValidFolderName(name), valid);
    }

    void sanitizedRepairsStoredValues()
    {
        Settings stored;
        stored.folders = {u"a/b"_s, u"../up"_s, u"_ausschuss"_s};
        stored.topN = 0;
        stored.minDuels = 50;
        stored.confidenceThreshold = 2;
        stored.sidecarExtensions = {u" txt "_s, u""_s, u"TXT"_s, u"tar.gz"_s, u"json"_s};

        const Settings fixed = sanitized(stored);
        QVERIFY(fixed.folders.keep.isEmpty());
        QCOMPARE(fixed.folders.rest, u"_rest"_s);
        QCOMPARE(fixed.folders.reject, u"_ausschuss"_s);
        QCOMPARE(fixed.topN, 1);
        QCOMPARE(fixed.minDuels, 10);
        QCOMPARE(fixed.confidenceThreshold, 0.5);
        QCOMPARE(fixed.sidecarExtensions, (QStringList{u"txt"_s, u"json"_s}));
        QCOMPARE(sanitized(Settings{}), Settings{});
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
