// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QHash>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>
#include <QRunnable>
#include <QSize>
#include <QThreadPool>
#include <QUrl>
#include <QWaitCondition>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QQmlEngine;
class QJSEngine;

// Decoded images scaled to their display size, shared by the image provider and prefetching.
// QML's own pixmap cache drops unreferenced images too early for full-size photos.
class ImageCache : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // No default arguments: QML would otherwise construct its own instance instead of calling create().
    explicit ImageCache(qint64 maxBytes, QObject *parent = nullptr);
    ~ImageCache() override;

    static void setInstance(ImageCache *cache);
    static ImageCache *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Blocks until the image is decoded. Waits for a running prefetch instead of decoding twice.
    QImage obtain(const QString &path, const QSize &size);

    Q_INVOKABLE void prefetch(const QList<QUrl> &urls, const QSize &size);

private:
    struct Entry {
        QImage image;
        bool ready = false;
        quint64 lastUse = 0;
    };
    using Key = QPair<QString, QSize>;

    static QImage decode(const QString &path, const QSize &size);
    // Requires m_mutex. A finished entry of the same file at least as large as size, or null.
    std::shared_ptr<Entry> largerEntry(const QString &path, const QSize &size) const;
    void store(const Key &key, const QImage &image);
    void evict();

    QMutex m_mutex;
    QWaitCondition m_readyCondition;
    QHash<Key, std::shared_ptr<Entry>> m_entries;
    quint64 m_clock = 0;
    qint64 m_maxBytes;
    QThreadPool m_prefetchPool;
};

// Serves images from the cache on its own threads, so the two duel images load in parallel.
class CachedImageProvider : public QQuickAsyncImageProvider
{
public:
    explicit CachedImageProvider(ImageCache *cache);
    ~CachedImageProvider() override;
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    ImageCache *m_cache;
    QThreadPool m_pool;
};
