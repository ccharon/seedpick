// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "imagecache.h"

#include "imageurl.h"

#include <QImageReader>
#include <QJSEngine>
#include <QThreadPool>

#include <algorithm>

namespace
{
ImageCache *s_instance = nullptr;
}

ImageCache::ImageCache(qint64 maxBytes, QObject *parent)
    : QObject(parent)
    , m_maxBytes(maxBytes)
{
}

ImageCache::~ImageCache()
{
    // Prefetch tasks reference this object.
    m_prefetchPool.waitForDone();
}

void ImageCache::setInstance(ImageCache *cache)
{
    s_instance = cache;
}

ImageCache *ImageCache::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QImage ImageCache::decode(const QString &path, const QSize &size)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QSize target = reader.size();
    if (reader.transformation() & QImageIOHandler::TransformationRotate90) {
        target.transpose();
    }
    // An empty size asks for the full resolution.
    if (target.isValid() && !size.isEmpty() && (target.width() > size.width() || target.height() > size.height())) {
        target.scale(size, Qt::KeepAspectRatio);
        if (reader.transformation() & QImageIOHandler::TransformationRotate90) {
            target.transpose();
        }
        reader.setScaledSize(target);
    }
    return reader.read();
}

QImage ImageCache::obtain(const QString &path, const QSize &size)
{
    const Key key{path, size};
    QMutexLocker locker(&m_mutex);
    if (const std::shared_ptr<Entry> entry = m_entries.value(key)) {
        while (!entry->ready) {
            m_readyCondition.wait(&m_mutex);
        }
        entry->lastUse = ++m_clock;
        return entry->image;
    }
    // Thumbnails are scaled down from an image that is already decoded.
    const std::shared_ptr<Entry> larger = largerEntry(path, size);
    m_entries.insert(key, std::make_shared<Entry>());
    locker.unlock();

    const QImage image = larger ? larger->image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation) : decode(path, size);
    store(key, image);
    return image;
}

void ImageCache::prefetch(const QList<QUrl> &urls, const QSize &size)
{
    if (size.isEmpty()) {
        return;
    }
    for (const QUrl &url : urls) {
        const QString path = ImageUrl::pathFromUrl(url);
        if (path.isEmpty()) {
            continue;
        }
        const Key key{path, size};
        {
            QMutexLocker locker(&m_mutex);
            if (const std::shared_ptr<Entry> entry = m_entries.value(key)) {
                // Recently requested images must survive eviction.
                entry->lastUse = ++m_clock;
                continue;
            }
            m_entries.insert(key, std::make_shared<Entry>());
        }
        m_prefetchPool.start([this, key]() {
            store(key, decode(key.first, key.second));
        });
    }
}

std::shared_ptr<ImageCache::Entry> ImageCache::largerEntry(const QString &path, const QSize &size) const
{
    if (size.isEmpty()) {
        return nullptr;
    }
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        const QImage &image = it.value()->image;
        if (it.key().first == path && it.value()->ready && !image.isNull() && (image.width() >= size.width() || image.height() >= size.height())) {
            return it.value();
        }
    }
    return nullptr;
}

void ImageCache::store(const Key &key, const QImage &image)
{
    QMutexLocker locker(&m_mutex);
    std::shared_ptr<Entry> entry = m_entries.value(key);
    if (!entry) {
        entry = std::make_shared<Entry>();
        m_entries.insert(key, entry);
    }
    entry->image = image;
    entry->ready = true;
    entry->lastUse = ++m_clock;
    m_readyCondition.wakeAll();
    evict();
}

// Requires m_mutex. Drops the least recently used finished entries until the budget fits.
void ImageCache::evict()
{
    qint64 total = 0;
    QList<QPair<quint64, Key>> finished;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        if (it.value()->ready) {
            total += it.value()->image.sizeInBytes();
            finished.append({it.value()->lastUse, it.key()});
        }
    }
    std::sort(finished.begin(), finished.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
    });
    // The newest entry always stays, even when it alone exceeds the budget.
    for (qsizetype i = 0; i + 1 < finished.size() && total > m_maxBytes; ++i) {
        total -= m_entries.value(finished.at(i).second)->image.sizeInBytes();
        m_entries.remove(finished.at(i).second);
    }
}

namespace
{
class CachedImageResponse : public QQuickImageResponse, public QRunnable
{
public:
    CachedImageResponse(ImageCache *cache, const QString &path, const QSize &size)
        : m_cache(cache)
        , m_path(path)
        , m_size(size)
    {
        // QML owns and deletes the response.
        setAutoDelete(false);
    }

    void run() override
    {
        m_image = m_cache->obtain(m_path, m_size);
        Q_EMIT finished();
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

private:
    ImageCache *m_cache;
    QString m_path;
    QSize m_size;
    QImage m_image;
};
}

CachedImageProvider::CachedImageProvider(ImageCache *cache)
    : m_cache(cache)
{
}

CachedImageProvider::~CachedImageProvider()
{
    m_pool.waitForDone();
}

QQuickImageResponse *CachedImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    auto *response = new CachedImageResponse(m_cache, ImageUrl::pathFromId(id), requestedSize);
    m_pool.start(response);
    return response;
}
