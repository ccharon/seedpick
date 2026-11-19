// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QtEndian>

#include "imagemetadata.h"

using namespace Seedpick;
using namespace Qt::Literals::StringLiterals;

namespace
{
QByteArray uint32(quint32 value)
{
    QByteArray bytes(4, '\0');
    qToBigEndian(value, bytes.data());
    return bytes;
}

// The reader skips CRCs, so they stay zero.
QByteArray chunk(const QByteArray &type, const QByteArray &data)
{
    return uint32(quint32(data.size())) + type + data + uint32(0);
}

QByteArray zlib(const QByteArray &data)
{
    // qCompress puts a 4 byte length in front of the zlib stream.
    return qCompress(data).mid(4);
}

QByteArray png(const QList<QByteArray> &chunks)
{
    QByteArray header = uint32(640) + uint32(480) + QByteArray("\x08\x06\x00\x00\x00", 5);
    QByteArray data = QByteArray("\x89PNG\r\n\x1a\n", 8) + chunk("IHDR", header);
    for (const QByteArray &c : chunks) {
        data += c;
    }
    return data + chunk("IDAT", QByteArray(100, 'x')) + chunk("IEND", {});
}

std::optional<PngInfo> read(QByteArray data)
{
    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);
    return readPngInfo(buffer);
}

QString setting(const GenerationInfo &info, const QString &label)
{
    for (const auto &[key, value] : info.settings) {
        if (key == label) {
            return value;
        }
    }
    return {};
}

QString json(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QJsonObject node(const QString &type, const QJsonObject &inputs)
{
    return {{"class_type"_L1, type}, {"inputs"_L1, inputs}};
}

QJsonArray link(const char *id)
{
    return {QString::fromLatin1(id), 0};
}
}

class TestImageMetadata : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsAllTextChunks()
    {
        const QByteArray utf8 = u"Häschen 🐇"_s.toUtf8();
        const auto info = read(png({
            chunk("tEXt", "plain\0value"_ba),
            chunk("zTXt", "packed\0\0"_ba + zlib(QByteArray(5000, 'z'))),
            chunk("iTXt", "intl\0\0\0de\0Titel\0"_ba + utf8),
            chunk("iTXt", "intlz\0\1\0\0\0"_ba + zlib(utf8)),
        }));
        QVERIFY(info);
        QCOMPARE(info->size, QSize(640, 480));
        QCOMPARE(info->texts.size(), 4);
        QCOMPARE(info->texts.at(0), (PngText{u"plain"_s, u"value"_s}));
        QCOMPARE(info->texts.at(1), (PngText{u"packed"_s, QString(5000, u'z')}));
        QCOMPARE(info->texts.at(2), (PngText{u"intl"_s, u"Häschen 🐇"_s}));
        QCOMPARE(info->texts.at(3), (PngText{u"intlz"_s, u"Häschen 🐇"_s}));
    }

    void textAfterImageDataIsFound()
    {
        QByteArray data = png({});
        data.insert(data.size() - 12, chunk("tEXt", "late\0yes"_ba));
        const auto info = read(data);
        QVERIFY(info);
        QCOMPARE(info->texts, (QList<PngText>{{u"late"_s, u"yes"_s}}));
    }

    void otherFilesAreNotPng()
    {
        QVERIFY(!read("\xff\xd8\xff\xe0 jpeg"_ba));
        QVERIFY(!read({}));
    }

    void brokenChunksAreSkipped()
    {
        const auto info = read(png({chunk("tEXt", "nokeyword"_ba), chunk("zTXt", "bad\0\0garbage"_ba), chunk("tEXt", "good\0ok"_ba)}));
        QVERIFY(info);
        QCOMPARE(info->texts, (QList<PngText>{{u"good"_s, u"ok"_s}}));
    }

    void a1111Parameters()
    {
        const QString parameters =
            u"a rabbit,\nin a garden\nNegative prompt: blurry,\nugly\n"
            u"Steps: 20, Sampler: DPM++ 2M, CFG scale: 7, Seed: 42, Size: 512x768, Model: dream, "
            u"Lora hashes: \"fur: 123, light: 456\", Version: v1.10"_s;
        const GenerationInfo info = parseGenerationInfo({{u"parameters"_s, parameters}});
        QCOMPARE(info.generator, u"A1111"_s);
        QCOMPARE(info.prompt, u"a rabbit,\nin a garden"_s);
        QCOMPARE(info.negativePrompt, u"blurry,\nugly"_s);
        QCOMPARE(setting(info, u"Steps"_s), u"20"_s);
        QCOMPARE(setting(info, u"Sampler"_s), u"DPM++ 2M"_s);
        QCOMPARE(setting(info, u"Seed"_s), u"42"_s);
        QCOMPARE(setting(info, u"Lora hashes"_s), u"fur: 123, light: 456"_s);
        QCOMPARE(setting(info, u"Version"_s), u"v1.10"_s);
        QCOMPARE(info.settings.first().first, u"Steps"_s);
    }

    void a1111PromptOnly()
    {
        const GenerationInfo info = parseGenerationInfo({{u"parameters"_s, u"just a prompt"_s}});
        QCOMPARE(info.prompt, u"just a prompt"_s);
        QVERIFY(info.negativePrompt.isEmpty());
        QVERIFY(info.settings.isEmpty());
    }

    void comfyUiSimpleGraph()
    {
        const QJsonObject graph{
            {"4"_L1, node(u"CheckpointLoaderSimple"_s, {{"ckpt_name"_L1, "sdxl.safetensors"_L1}})},
            {"6"_L1, node(u"CLIPTextEncode"_s, {{"text"_L1, "a rabbit"_L1}, {"clip"_L1, link("4")}})},
            {"7"_L1, node(u"CLIPTextEncode"_s, {{"text"_L1, "blurry"_L1}, {"clip"_L1, link("4")}})},
            {"3"_L1,
             node(u"KSampler"_s,
                  {{"seed"_L1, 123456789012.0},
                   {"steps"_L1, 30},
                   {"cfg"_L1, 6.5},
                   {"sampler_name"_L1, "euler"_L1},
                   {"scheduler"_L1, "karras"_L1},
                   {"denoise"_L1, 1},
                   {"model"_L1, link("4")},
                   {"positive"_L1, link("6")},
                   {"negative"_L1, link("7")}})},
        };
        const GenerationInfo info = parseGenerationInfo({{u"prompt"_s, json(graph)}, {u"workflow"_s, u"{}"_s}});
        QCOMPARE(info.generator, u"ComfyUI"_s);
        QCOMPARE(info.prompt, u"a rabbit"_s);
        QCOMPARE(info.negativePrompt, u"blurry"_s);
        const QList<std::pair<QString, QString>> expected{
            {u"Seed"_s, u"123456789012"_s},
            {u"Steps"_s, u"30"_s},
            {u"CFG"_s, u"6.5"_s},
            {u"Sampler"_s, u"euler"_s},
            {u"Scheduler"_s, u"karras"_s},
            {u"Denoise"_s, u"1"_s},
            {u"Model"_s, u"sdxl.safetensors"_s},
        };
        QCOMPARE(info.settings, expected);
    }

    void comfyUiFollowsLinks()
    {
        // Shaped like a real workflow: the prompt goes through an LLM node, the seed comes
        // from a seed node, the negative side is zeroed out and a LoRA loader sits before the model.
        const QJsonObject graph{
            {"1"_L1, node(u"UNETLoader"_s, {{"unet_name"_L1, "krea.safetensors"_L1}})},
            {"5"_L1, node(u"CLIPTextEncode"_s, {{"text"_L1, link("166")}})},
            {"8"_L1,
             node(u"KSampler"_s,
                  {{"seed"_L1, link("192")}, {"steps"_L1, 10}, {"model"_L1, link("31")}, {"positive"_L1, link("5")}, {"negative"_L1, link("104")}})},
            {"31"_L1,
             node(u"Power Lora Loader (rgthree)"_s,
                  {{"lora_1"_L1, QJsonObject{{"on"_L1, true}, {"lora"_L1, "realism.safetensors"_L1}, {"strength"_L1, 0.65}}},
                   {"lora_2"_L1, QJsonObject{{"on"_L1, false}, {"lora"_L1, "off.safetensors"_L1}, {"strength"_L1, 1}}},
                   {"model"_L1, link("1")}})},
            {"104"_L1, node(u"ConditioningZeroOut"_s, {{"conditioning"_L1, link("5")}})},
            {"110"_L1, node(u"StringFormat"_s, {{"f_string"_L1, "a rabbit eating cabbage"_L1}})},
            {"166"_L1, node(u"OllamaGenerateV2"_s, {{"system"_L1, "You write prompts"_L1}, {"prompt"_L1, link("110")}})},
            {"192"_L1, node(u"Seed (rgthree)"_s, {{"seed"_L1, 598520938392025.0}})},
        };
        const GenerationInfo info = parseGenerationInfo({{u"prompt"_s, json(graph)}});
        QCOMPARE(info.prompt, u"a rabbit eating cabbage"_s);
        QVERIFY(info.negativePrompt.isEmpty());
        QCOMPARE(setting(info, u"Seed"_s), u"598520938392025"_s);
        QCOMPARE(setting(info, u"Model"_s), u"krea.safetensors"_s);
        QCOMPARE(setting(info, u"LoRA"_s), u"realism.safetensors (0.65)"_s);
    }

    void plainPromptAndOtherTexts()
    {
        const GenerationInfo info = parseGenerationInfo({
            {u"prompt"_s, u"not json"_s},
            {u"Software"_s, u"Tool 1.0"_s},
            {u"Comment"_s, QString(600, u'c')},
        });
        QVERIFY(info.generator.isEmpty());
        QCOMPARE(info.prompt, u"not json"_s);
        QCOMPARE(setting(info, u"Software"_s), u"Tool 1.0"_s);
        QCOMPARE(setting(info, u"Comment"_s).size(), 501);
    }

    void nothingKnown()
    {
        QVERIFY(parseGenerationInfo({}).isEmpty());
        QVERIFY(parseGenerationInfo({{u"workflow"_s, u"{}"_s}}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestImageMetadata)
#include "tst_imagemetadata.moc"
