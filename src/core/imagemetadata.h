// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QList>
#include <QSize>
#include <QString>

#include <optional>
#include <utility>

class QIODevice;

namespace Seedpick
{
struct PngText {
    QString keyword;
    QString text;

    bool operator==(const PngText &) const = default;
};

struct PngInfo {
    QSize size;
    QList<PngText> texts;
};

// Reads the image size and the text chunks (tEXt, zTXt, iTXt) of a PNG file.
// Returns std::nullopt for files that are not PNG.
std::optional<PngInfo> readPngInfo(QIODevice &device);
std::optional<PngInfo> readPngInfo(const QString &path);

// What an image generator stored about how the image was made.
struct GenerationInfo {
    // "ComfyUI" or "A1111", empty when no known format was found.
    QString generator;
    QString prompt;
    QString negativePrompt;
    // Label and value in display order, for example ("Seed", "1234").
    QList<std::pair<QString, QString>> settings;

    bool isEmpty() const
    {
        return generator.isEmpty() && prompt.isEmpty() && negativePrompt.isEmpty() && settings.isEmpty();
    }
};

// Understands the A1111 "parameters" text and the ComfyUI "prompt" graph. In a ComfyUI graph
// the prompt is followed through links to the first literal text, which can be the input of
// a node that rewrites the prompt at run time. Other text chunks become settings, the large
// ComfyUI "workflow" chunk is left out.
GenerationInfo parseGenerationInfo(const QList<PngText> &texts);
}
