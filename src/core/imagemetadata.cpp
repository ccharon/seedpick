// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "imagemetadata.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QtEndian>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace Seedpick
{
namespace
{
constexpr char pngSignature[] = "\x89PNG\r\n\x1a\n";
// Text chunks above this size are skipped, real prompts and workflows stay far below.
constexpr quint32 maxTextChunk = 32 * 1024 * 1024;
constexpr int maxLinkDepth = 32;
constexpr qsizetype maxOtherTextLength = 500;

std::optional<quint32> readUInt32(QIODevice &device)
{
    const QByteArray bytes = device.read(4);
    if (bytes.size() != 4) {
        return std::nullopt;
    }
    return qFromBigEndian<quint32>(bytes.constData());
}

// zlib stream without Qt's 4 byte length prefix.
std::optional<QByteArray> inflate(const QByteArray &data)
{
    QByteArray prefixed(4, '\0');
    qToBigEndian<quint32>(quint32(std::min<qsizetype>(data.size() * 8, maxTextChunk)), prefixed.data());
    prefixed.append(data);
    const QByteArray result = qUncompress(prefixed);
    if (result.isEmpty() && !data.isEmpty()) {
        return std::nullopt;
    }
    return result;
}

std::optional<PngText> parseTextChunk(const QByteArray &type, const QByteArray &data)
{
    const qsizetype end = data.indexOf('\0');
    if (end <= 0) {
        return std::nullopt;
    }
    const QString keyword = QString::fromLatin1(data.left(end));
    const QByteArray rest = data.mid(end + 1);
    if (type == "tEXt") {
        return PngText{keyword, QString::fromLatin1(rest)};
    }
    if (type == "zTXt") {
        if (rest.isEmpty() || rest.at(0) != 0) {
            return std::nullopt;
        }
        const std::optional<QByteArray> text = inflate(rest.mid(1));
        return text ? std::optional(PngText{keyword, QString::fromLatin1(*text)}) : std::nullopt;
    }
    // iTXt: compression flag, method, language tag, translated keyword, UTF-8 text.
    if (rest.size() < 2) {
        return std::nullopt;
    }
    const bool compressed = rest.at(0) != 0;
    const qsizetype languageEnd = rest.indexOf('\0', 2);
    const qsizetype translatedEnd = languageEnd < 0 ? -1 : rest.indexOf('\0', languageEnd + 1);
    if (translatedEnd < 0) {
        return std::nullopt;
    }
    QByteArray text = rest.mid(translatedEnd + 1);
    if (compressed) {
        const std::optional<QByteArray> inflated = inflate(text);
        if (!inflated) {
            return std::nullopt;
        }
        text = *inflated;
    }
    return PngText{keyword, QString::fromUtf8(text)};
}

// "Steps: 20, Sampler: Euler a, Lora hashes: "a: 1, b: 2"" into label/value pairs.
QList<std::pair<QString, QString>> parseA1111Settings(const QString &line)
{
    QList<std::pair<QString, QString>> settings;
    qsizetype pos = 0;
    while (pos < line.size()) {
        const qsizetype colon = line.indexOf(": "_L1, pos);
        if (colon < 0) {
            break;
        }
        const QString label = line.mid(pos, colon - pos).trimmed();
        qsizetype valueStart = colon + 2;
        qsizetype valueEnd;
        QString value;
        if (valueStart < line.size() && line.at(valueStart) == u'"') {
            valueEnd = line.indexOf(u'"', valueStart + 1);
            if (valueEnd < 0) {
                valueEnd = line.size();
            }
            value = line.mid(valueStart + 1, valueEnd - valueStart - 1);
            valueEnd = line.indexOf(", "_L1, valueEnd);
        } else {
            valueEnd = line.indexOf(", "_L1, valueStart);
            value = line.mid(valueStart, (valueEnd < 0 ? line.size() : valueEnd) - valueStart);
        }
        settings.append({label, value.trimmed()});
        if (valueEnd < 0) {
            break;
        }
        pos = valueEnd + 2;
    }
    return settings;
}

GenerationInfo parseA1111(const QString &parameters)
{
    GenerationInfo info;
    info.generator = u"A1111"_s;
    QStringList lines = parameters.split(u'\n');
    // The last line holds the settings when it starts with "Steps:".
    if (!lines.isEmpty() && lines.last().trimmed().startsWith("Steps: "_L1)) {
        info.settings = parseA1111Settings(lines.takeLast().trimmed());
    }
    const qsizetype negative = std::find_if(lines.cbegin(),
                                            lines.cend(),
                                            [](const QString &line) {
                                                return line.startsWith("Negative prompt:"_L1);
                                            })
        - lines.cbegin();
    info.prompt = lines.mid(0, negative).join(u'\n').trimmed();
    if (negative < lines.size()) {
        QStringList negativeLines = lines.mid(negative);
        negativeLines.first() = negativeLines.first().mid(QStringLiteral("Negative prompt:").size());
        info.negativePrompt = negativeLines.join(u'\n').trimmed();
    }
    return info;
}

class ComfyGraph
{
public:
    explicit ComfyGraph(const QJsonObject &graph)
        : m_graph(graph)
    {
    }

    // Node ids sorted numerically, so the first matching node is stable.
    QStringList nodeIds() const
    {
        QStringList ids = m_graph.keys();
        std::sort(ids.begin(), ids.end(), [](const QString &a, const QString &b) {
            return a.toLongLong() < b.toLongLong();
        });
        return ids;
    }

    QJsonObject node(const QString &id) const
    {
        return m_graph.value(id).toObject();
    }

    static QJsonObject inputs(const QJsonObject &node)
    {
        return node.value("inputs"_L1).toObject();
    }

    static bool isLink(const QJsonValue &value)
    {
        const QJsonArray array = value.toArray();
        return value.isArray() && array.size() == 2 && array.at(0).isString();
    }

    QJsonObject linked(const QJsonValue &value) const
    {
        return isLink(value) ? node(value.toArray().at(0).toString()) : QJsonObject();
    }

    // The first literal reachable through the given input names.
    QJsonValue resolve(const QJsonValue &value, const QStringList &keys, int depth = 0) const
    {
        if (!isLink(value)) {
            return value.isArray() || value.isObject() ? QJsonValue(QJsonValue::Undefined) : value;
        }
        if (depth > maxLinkDepth) {
            return QJsonValue::Undefined;
        }
        const QJsonObject nodeInputs = inputs(linked(value));
        for (const QString &key : keys) {
            if (nodeInputs.contains(key)) {
                const QJsonValue result = resolve(nodeInputs.value(key), keys, depth + 1);
                if (!result.isUndefined()) {
                    return result;
                }
            }
        }
        return QJsonValue::Undefined;
    }

    // Text of the encoder behind a sampler's positive or negative input.
    QString encoderText(const QJsonValue &link, int depth = 0) const
    {
        static const QStringList textKeys{u"text"_s, u"prompt"_s, u"f_string"_s, u"string"_s, u"text_g"_s, u"value"_s};
        static const QStringList conditioningKeys{u"conditioning"_s, u"conditioning_1"_s, u"conditioning_to"_s, u"conditioning_from"_s};
        const QJsonObject encoder = linked(link);
        if (encoder.isEmpty() || depth > maxLinkDepth || encoder.value("class_type"_L1).toString().contains("ZeroOut"_L1)) {
            return {};
        }
        const QJsonObject encoderInputs = inputs(encoder);
        for (const QString &key : {u"text"_s, u"text_g"_s, u"prompt"_s}) {
            if (encoderInputs.contains(key)) {
                return resolve(encoderInputs.value(key), textKeys).toString().trimmed();
            }
        }
        for (const QString &key : conditioningKeys) {
            if (isLink(encoderInputs.value(key))) {
                return encoderText(encoderInputs.value(key), depth + 1);
            }
        }
        return {};
    }

private:
    QJsonObject m_graph;
};

QString literalText(const QJsonValue &value)
{
    if (value.isDouble()) {
        const double number = value.toDouble();
        return number == double(qint64(number)) ? QString::number(qint64(number)) : QString::number(number);
    }
    if (value.isBool()) {
        return value.toBool() ? u"true"_s : u"false"_s;
    }
    return value.toString();
}

GenerationInfo parseComfyUi(const QJsonObject &graphObject)
{
    const ComfyGraph graph(graphObject);
    GenerationInfo info;
    info.generator = u"ComfyUI"_s;

    QJsonObject sampler;
    for (const QString &id : graph.nodeIds()) {
        const QJsonObject node = graph.node(id);
        const QJsonObject nodeInputs = ComfyGraph::inputs(node);
        const bool hasConditioning = ComfyGraph::isLink(nodeInputs.value("positive"_L1));
        if (hasConditioning && node.value("class_type"_L1).toString().contains("KSampler"_L1)) {
            sampler = node;
            break;
        }
        if (hasConditioning && sampler.isEmpty()) {
            sampler = node;
        }
    }
    if (sampler.isEmpty()) {
        return info;
    }

    const QJsonObject samplerInputs = ComfyGraph::inputs(sampler);
    info.prompt = graph.encoderText(samplerInputs.value("positive"_L1));
    info.negativePrompt = graph.encoderText(samplerInputs.value("negative"_L1));

    const std::pair<QString, QStringList> fields[] = {
        {u"Seed"_s, {u"seed"_s, u"noise_seed"_s, u"value"_s, u"int"_s}},
        {u"Steps"_s, {u"steps"_s}},
        {u"CFG"_s, {u"cfg"_s}},
        {u"Sampler"_s, {u"sampler_name"_s}},
        {u"Scheduler"_s, {u"scheduler"_s}},
        {u"Denoise"_s, {u"denoise"_s}},
    };
    for (const auto &[label, keys] : fields) {
        for (const QString &key : keys) {
            if (samplerInputs.contains(key)) {
                const QJsonValue value = graph.resolve(samplerInputs.value(key), keys);
                if (!value.isUndefined()) {
                    info.settings.append({label, literalText(value)});
                }
                break;
            }
        }
    }

    // Walk the model input back to the loader, collecting LoRAs on the way.
    QStringList loras;
    QString model;
    QJsonValue link = samplerInputs.value("model"_L1);
    for (int depth = 0; depth < maxLinkDepth && ComfyGraph::isLink(link); ++depth) {
        const QJsonObject nodeInputs = ComfyGraph::inputs(graph.linked(link));
        for (const QString &key : {u"ckpt_name"_s, u"unet_name"_s, u"model_name"_s}) {
            if (nodeInputs.value(key).isString()) {
                model = nodeInputs.value(key).toString();
            }
        }
        if (nodeInputs.value("lora_name"_L1).isString()) {
            loras.append(u"%1 (%2)"_s.arg(nodeInputs.value("lora_name"_L1).toString(), literalText(nodeInputs.value("strength_model"_L1))));
        }
        for (auto it = nodeInputs.constBegin(); it != nodeInputs.constEnd(); ++it) {
            const QJsonObject entry = it.value().toObject();
            if (it.key().startsWith("lora_"_L1) && entry.value("on"_L1).toBool() && entry.value("lora"_L1).isString()) {
                loras.append(u"%1 (%2)"_s.arg(entry.value("lora"_L1).toString(), literalText(entry.value("strength"_L1))));
            }
        }
        if (!model.isEmpty()) {
            break;
        }
        link = nodeInputs.value("model"_L1);
    }
    if (!model.isEmpty()) {
        info.settings.append({u"Model"_s, model});
    }
    if (!loras.isEmpty()) {
        info.settings.append({u"LoRA"_s, loras.join(", "_L1)});
    }
    return info;
}
}

std::optional<PngInfo> readPngInfo(QIODevice &device)
{
    if (device.read(8) != QByteArray(pngSignature, 8)) {
        return std::nullopt;
    }
    PngInfo info;
    while (!device.atEnd()) {
        const std::optional<quint32> length = readUInt32(device);
        const QByteArray type = device.read(4);
        if (!length || type.size() != 4) {
            break;
        }
        if (type == "IHDR" && *length >= 8) {
            const QByteArray header = device.read(*length);
            info.size = QSize(int(qFromBigEndian<quint32>(header.constData())), int(qFromBigEndian<quint32>(header.constData() + 4)));
        } else if ((type == "tEXt" || type == "zTXt" || type == "iTXt") && *length <= maxTextChunk) {
            if (const std::optional<PngText> text = parseTextChunk(type, device.read(*length))) {
                info.texts.append(*text);
            }
        } else if (type == "IEND") {
            break;
        } else if (!device.seek(device.pos() + *length)) {
            break;
        }
        // CRC.
        if (!device.seek(device.pos() + 4)) {
            break;
        }
    }
    return info;
}

std::optional<PngInfo> readPngInfo(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return readPngInfo(file);
}

GenerationInfo parseGenerationInfo(const QList<PngText> &texts)
{
    GenerationInfo info;
    QList<std::pair<QString, QString>> others;
    for (const PngText &text : texts) {
        if (text.keyword == "parameters"_L1 && info.generator.isEmpty()) {
            info = parseA1111(text.text);
        } else if (text.keyword == "prompt"_L1 && info.generator.isEmpty()) {
            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(text.text.toUtf8(), &error);
            if (error.error == QJsonParseError::NoError && document.isObject()) {
                info = parseComfyUi(document.object());
            } else if (info.prompt.isEmpty()) {
                info.prompt = text.text.trimmed();
            }
        } else if (text.keyword != "workflow"_L1) {
            others.append({text.keyword, text.text.size() > maxOtherTextLength ? text.text.left(maxOtherTextLength) + u'…' : text.text});
        }
    }
    info.settings.append(others);
    return info;
}
}
