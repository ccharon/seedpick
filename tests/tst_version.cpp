// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QRegularExpression>
#include <QTest>

#include "version.h"

class TestVersion : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void projectVersionIsSemver()
    {
        static const QRegularExpression semver(QStringLiteral(R"(^\d+\.\d+\.\d+$)"));
        QVERIFY2(semver.match(Seedpick::projectVersion()).hasMatch(), qPrintable(Seedpick::projectVersion()));
    }

    void versionStringStartsWithProjectVersion()
    {
        QVERIFY(Seedpick::versionString().startsWith(Seedpick::projectVersion()));
    }

    void appIdIsReverseDns()
    {
        QCOMPARE(Seedpick::appId(), QStringLiteral("io.github.ccharon.seedpick"));
    }
};

QTEST_GUILESS_MAIN(TestVersion)
#include "tst_version.moc"
