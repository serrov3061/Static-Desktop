/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_banner.h"

#include "staticgram/sg_badges.h"
#include "staticgram/sg_badges_bot.h"
#include "apiwrap.h"
#include "base/random.h"
#include "base/unixtime.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "media/clip/media_clip_reader.h"
#include "settings.h"
#include "storage/file_upload.h"
#include "storage/localimageloader.h"
#include "ui/chat/attach/attach_prepare.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtGui/QImageReader>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace StaticGram {
namespace {

constexpr auto kMaxDownload = qint64(11) * 1024 * 1024;
constexpr auto kRetryAfter = crl::time(60 * 1000);
constexpr auto kMaxDecodedWidth = 1600;
constexpr auto kMaxUploadWidth = 1920;
constexpr auto kMaxPhotoSource = qint64(64) * 1024 * 1024;

[[nodiscard]] QString CacheDir() {
	return cWorkingDir() + u"tdata/sg_banners/"_q;
}

[[nodiscard]] QString CachePath(const QString &id, bool video) {
	return CacheDir() + id + (video ? u".mp4"_q : u".jpg"_q);
}

// Runs off the main thread.
[[nodiscard]] QImage DecodeBanner(const QString &path, bool video) {
	auto result = QImage();
	if (video) {
		auto info = ::Media::Clip::PrepareForSending(path, QByteArray());
		if (const auto media = std::get_if<
				Ui::PreparedFileInformation::Video>(&info.media)) {
			result = media->thumbnail;
		}
	} else {
		auto reader = QImageReader(path);
		reader.setAutoTransform(true);
		result = reader.read();
	}
	if (result.isNull()) {
		return QImage();
	}
	if (result.width() > kMaxDecodedWidth) {
		result = result.scaledToWidth(
			kMaxDecodedWidth,
			Qt::SmoothTransformation);
	}
	return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

class Images final {
public:
	[[nodiscard]] QImage image(uint64 userId) {
		const auto banner = BannerOfUser(userId);
		if (!banner) {
			return QImage();
		}
		const auto i = _ready.constFind(banner.id);
		if (i != _ready.cend()) {
			return i.value();
		}
		load(banner);
		return QImage();
	}
	void clear() {
		_ready.clear();
	}

private:
	void load(const ProfileBanner &banner) {
		const auto id = banner.id;
		const auto video = banner.video;
		if (_loading.contains(id)) {
			return;
		}
		const auto failed = _failed.constFind(id);
		if (failed != _failed.cend() && crl::now() - failed.value() < kRetryAfter) {
			return;
		}
		_loading.insert(id);
		const auto path = CachePath(id, video);
		crl::async([=] {
			auto image = QImage();
			if (QFileInfo(path).size() > 0) {
				image = DecodeBanner(path, video);
			}
			crl::on_main([=, image = std::move(image)]() mutable {
				if (!image.isNull()) {
					finish(id, std::move(image));
				} else {
					download(id, video);
				}
			});
		});
	}
	void download(const QString &id, bool video) {
		if (!_network) {
			_network = std::make_unique<QNetworkAccessManager>();
		}
		auto request = QNetworkRequest(
			QUrl(BadgesApiBase() + u"banners/"_q + id));
		request.setTransferTimeout(30000);
		const auto reply = _network->get(request);
		QObject::connect(reply, &QNetworkReply::finished, [=] {
			reply->deleteLater();
			const auto status = reply->attribute(
				QNetworkRequest::HttpStatusCodeAttribute).toInt();
			if (reply->error() != QNetworkReply::NoError || status != 200) {
				fail(id);
				return;
			}
			const auto data = reply->read(kMaxDownload + 1);
			if (data.isEmpty() || data.size() > kMaxDownload) {
				fail(id);
				return;
			}
			QDir().mkpath(CacheDir());
			const auto path = CachePath(id, video);
			auto file = QFile(path);
			if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
				|| file.write(data) != data.size()) {
				file.close();
				QFile::remove(path);
				fail(id);
				return;
			}
			file.close();
			crl::async([=] {
				auto image = DecodeBanner(path, video);
				crl::on_main([=, image = std::move(image)]() mutable {
					if (image.isNull()) {
						QFile::remove(path);
						fail(id);
					} else {
						finish(id, std::move(image));
					}
				});
			});
		});
	}
	void fail(const QString &id) {
		_loading.remove(id);
		_failed.insert(id, crl::now());
	}
	void finish(const QString &id, QImage image) {
		_loading.remove(id);
		_failed.remove(id);
		if (_ready.size() >= 16) {
			_ready.clear();
		}
		_ready.insert(id, std::move(image));
		NotifyBadgesChanged();
	}

	QHash<QString, QImage> _ready;
	QSet<QString> _loading;
	QHash<QString, crl::time> _failed;
	std::unique_ptr<QNetworkAccessManager> _network;

};

[[nodiscard]] Images &Cache() {
	// Never destroyed: it may be used until the very end of the app.
	static const auto instance = new Images();
	return *instance;
}

struct Prepared {
	QByteArray bytes;
	QString mime;
	QString filename;
	bool video = false;
	double seconds = 0.;
	int width = 0;
	int height = 0;
	QString error; // "photo" or "video" when the file does not fit.
};

[[nodiscard]] Prepared PrepareVideo(const QString &path) {
	auto result = Prepared{ .video = true, .error = u"video"_q };
	const auto size = QFileInfo(path).size();
	if (size <= 0 || size > kBannerMaxVideoBytes) {
		return result;
	}
	auto info = ::Media::Clip::PrepareForSending(path, QByteArray());
	const auto media = std::get_if<Ui::PreparedFileInformation::Video>(
		&info.media);
	if (!media || media->duration <= 0 || media->thumbnail.isNull()) {
		return result;
	}
	const auto seconds = media->duration / 1000.;
	if (seconds > kBannerMaxVideoSeconds + 0.4) {
		return result;
	}
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return result;
	}
	result.bytes = file.readAll();
	if (result.bytes.size() != size) {
		result.bytes = QByteArray();
		return result;
	}
	result.mime = u"video/mp4"_q;
	result.filename = u"banner.mp4"_q;
	result.seconds = seconds;
	result.width = media->thumbnail.width();
	result.height = media->thumbnail.height();
	result.error = QString();
	return result;
}

[[nodiscard]] Prepared PreparePhoto(const QString &path) {
	auto result = Prepared{ .error = u"photo"_q };
	const auto size = QFileInfo(path).size();
	if (size <= 0 || size > kMaxPhotoSource) {
		return result;
	}
	auto reader = QImageReader(path);
	reader.setAutoTransform(true);
	auto image = reader.read();
	if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
		return result;
	}
	if (image.width() > kMaxUploadWidth) {
		image = image.scaledToWidth(
			kMaxUploadWidth,
			Qt::SmoothTransformation);
	}
	// A JPEG has no transparency: put the picture on white.
	auto flat = QImage(image.size(), QImage::Format_RGB32);
	flat.fill(Qt::white);
	{
		auto p = QPainter(&flat);
		p.drawImage(0, 0, image);
	}
	for (auto quality = 88; ; quality -= 12) {
		auto bytes = QByteArray();
		auto buffer = QBuffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		if (!flat.save(&buffer, "JPEG", quality)) {
			return result;
		}
		buffer.close();
		if (bytes.size() <= kBannerMaxPhotoBytes || quality <= 52) {
			if (bytes.size() > kBannerMaxPhotoBytes) {
				return result;
			}
			result.bytes = std::move(bytes);
			break;
		}
	}
	result.mime = u"image/jpeg"_q;
	result.filename = u"banner.jpg"_q;
	result.width = flat.width();
	result.height = flat.height();
	result.error = QString();
	return result;
}

[[nodiscard]] Prepared PrepareBanner(const QString &path) {
	const auto suffix = QFileInfo(path).suffix().toLower();
	const auto isVideo = (suffix == u"mp4"_q || suffix == u"m4v"_q);
	const auto isOtherVideo = (suffix == u"mov"_q)
		|| (suffix == u"webm"_q)
		|| (suffix == u"mkv"_q)
		|| (suffix == u"avi"_q)
		|| (suffix == u"gif"_q);
	if (isVideo) {
		return PrepareVideo(path);
	} else if (isOtherVideo) {
		return Prepared{ .video = true, .error = u"video"_q };
	}
	return PreparePhoto(path);
}

[[nodiscard]] std::shared_ptr<FilePrepareResult> MakeUploadFile(
		MTP::DcId dcId,
		const Prepared &prepared) {
	const auto id = base::RandomValue<DocumentId>();
	auto attributes = QVector<MTPDocumentAttribute>(
		1,
		MTP_documentAttributeFilename(MTP_string(prepared.filename)));
	auto result = MakePreparedFile({
		.id = id,
		.type = SendMediaType::File,
	});
	result->filename = prepared.filename;
	result->content = prepared.bytes;
	result->filesize = prepared.bytes.size();
	result->setFileData(prepared.bytes);
	result->document = MTP_document(
		MTP_flags(0),
		MTP_long(id),
		MTP_long(0),
		MTP_bytes(),
		MTP_int(base::unixtime::now()),
		MTP_string(prepared.mime),
		MTP_long(prepared.bytes.size()),
		MTP_vector<MTPPhotoSize>(),
		MTPVector<MTPVideoSize>(),
		MTP_int(dcId),
		MTP_vector<MTPDocumentAttribute>(std::move(attributes)));
	return result;
}

void SendUploaded(
		not_null<Main::Session*> session,
		not_null<UserData*> bot,
		const Prepared &prepared,
		const MTPInputFile &file,
		Fn<void(QString)> done) {
	using Flag = MTPDinputMediaUploadedDocument::Flag;
	auto attributes = QVector<MTPDocumentAttribute>();
	if (prepared.video) {
		auto videoFlags = MTPDdocumentAttributeVideo::Flags(
			MTPDdocumentAttributeVideo::Flag::f_supports_streaming);
		videoFlags |= MTPDdocumentAttributeVideo::Flag::f_nosound;
		attributes.push_back(MTP_documentAttributeVideo(
			MTP_flags(videoFlags),
			MTP_double(prepared.seconds),
			MTP_int(prepared.width),
			MTP_int(prepared.height),
			MTPint(), // preload_prefix_size
			MTPdouble(), // video_start_ts
			MTPstring())); // video_codec
	}
	attributes.push_back(
		MTP_documentAttributeFilename(MTP_string(prepared.filename)));
	const auto flags = prepared.video
		? Flag::f_nosound_video
		: Flag::f_force_file;
	const auto media = MTP_inputMediaUploadedDocument(
		MTP_flags(flags),
		file,
		MTPInputFile(),
		MTP_string(prepared.mime),
		MTP_vector<MTPDocumentAttribute>(std::move(attributes)),
		MTP_vector<MTPInputDocument>(),
		MTPInputPhoto(),
		MTP_int(0),
		MTP_int(0));
	const auto video = prepared.video;
	session->api().request(MTPmessages_UploadMedia(
		MTP_flags(0),
		MTPstring(),
		bot->input(),
		media
	)).done([=](const MTPMessageMedia &result) {
		auto documentId = DocumentId();
		auto accessHash = uint64();
		result.match([&](const MTPDmessageMediaDocument &data) {
			if (const auto document = data.vdocument()) {
				document->match([&](const MTPDdocument &fields) {
					documentId = fields.vid().v;
					accessHash = fields.vaccess_hash().v;
				}, [](const auto &) {
				});
			}
		}, [](const auto &) {
		});
		if (!documentId) {
			done(u"upload"_q);
			return;
		}
		ApplyBanner(session, documentId, accessHash, video, [=](QString r) {
			if (r == u"ok"_q) {
				ClearBannerImages();
				done(QString());
			} else {
				done(r.isEmpty() ? u"bot"_q : r);
			}
		});
	}).fail([=](const MTP::Error &error) {
		done(error.type().isEmpty() ? u"upload"_q : error.type());
	}).send();
}

} // namespace

QImage UserBannerImage(uint64 userId) {
	return Cache().image(userId);
}

void ClearBannerImages() {
	Cache().clear();
}

void UploadBanner(
		not_null<Main::Session*> session,
		const QString &path,
		Fn<void(QString)> done) {
	const auto weak = base::make_weak(session);
	crl::async([=] {
		auto prepared = PrepareBanner(path);
		crl::on_main(weak, [=, prepared = std::move(prepared)]() mutable {
			const auto strong = weak.get();
			if (!strong) {
				return;
			} else if (!prepared.error.isEmpty()) {
				done(prepared.error);
				return;
			}
			WithBadgesBot(strong, [=, prepared = std::move(prepared)](
					UserData *bot) mutable {
				const auto strong = weak.get();
				if (!strong) {
					return;
				} else if (!bot) {
					done(u"bot"_q);
					return;
				}
				const auto fakeId = FullMsgId(
					strong->userPeerId(),
					strong->data().nextLocalMessageId());
				const auto shared = std::make_shared<Prepared>(
					std::move(prepared));
				strong->uploader().documentReady(
				) | rpl::filter([=](const Storage::UploadedMedia &data) {
					return (data.fullId == fakeId);
				}) | rpl::take(1) | rpl::on_next([=](
						const Storage::UploadedMedia &data) {
					SendUploaded(
						strong,
						bot,
						*shared,
						data.info.file,
						done);
				}, strong->lifetime());
				strong->uploader().documentFailed(
				) | rpl::filter([=](const FullMsgId &id) {
					return (id == fakeId);
				}) | rpl::take(1) | rpl::on_next([=](const FullMsgId &) {
					done(u"upload"_q);
				}, strong->lifetime());
				strong->uploader().upload(
					fakeId,
					MakeUploadFile(strong->mainDcId(), *shared));
			});
		});
	});
}

void RemoveBanner(
		not_null<Main::Session*> session,
		Fn<void(QString)> done) {
	ApplyBanner(session, 0, 0, false, [=](QString result) {
		if (result == u"ok"_q) {
			ClearBannerImages();
			done(QString());
		} else {
			done(result.isEmpty() ? u"bot"_q : result);
		}
	});
}

} // namespace StaticGram
