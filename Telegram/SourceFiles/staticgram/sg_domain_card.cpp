/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_domain_card.h"

#include "ui/painter.h"
#include "styles/style_basic.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtGui/QLinearGradient>
#include <QtGui/QPainterPath>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtSvg/QSvgRenderer>

namespace StaticGram::Domains {
namespace {

// Android SGDomainCardDrawable, in its 1000x1000 units.
constexpr auto kUnits = 1000.;
constexpr auto kIconSize = 320.;
constexpr auto kIconRadius = 84.;
constexpr auto kIconY = 150.;
constexpr auto kPillCenterY = 640.;
constexpr auto kPillMaxWidth = 900.;
constexpr auto kPillMaxEm = 96;
constexpr auto kPillMinEm = 40;
constexpr auto kCaptionGap = 70.;
constexpr auto kTonBlue = 0xFF0098EA;

// The server PNG is 1000x1000, kept at the size Android asks ("280_280").
constexpr auto kRemoteSide = 280;
constexpr auto kMaxDownload = 4 * 1024 * 1024;
constexpr auto kRetryAfter = crl::time(60 * 1000);
constexpr auto kMaxCached = 64;

// R.drawable.staticgram_logo (25x20 dp vector), filled white the way the
// drawable is tinted with SRC_IN.
constexpr auto kLogoAspect = 25. / 20.;
const char kLogoPath[] =
	"M 227.988,2.594 c -13.1,3 -27.2,9.7 -37.9,17.8 -6.5,5 -30.3,24.7 "
	"-32.1,26.6 -0.3,0.3 -6.1,5.2 -13,10.9 -6.9,5.8 -17.7,14.8 -24,20.1 "
	"-6.3,5.3 -23.9,19.7 -39,32 -15.1,12.3 -30.2,24.5 -33.5,27.2 "
	"-3.3,2.7 -11.7,9.4 -18.6,14.8 -13.3,10.4 -19.3,16.9 -23.9,26 "
	"-4.4,8.7 -6,15.5 -6,25.4 0,10.3 1.5,16.9 5.8,25.9 3.4,7.1 3.6,7.3 "
	"19.6,27.3 6.6,8.2 19.9,24.8 29.6,36.9 23.5,29.4 54.2,67.4 "
	"65.9,81.5 5.2,6.3 12.1,14.6 15.3,18.5 35.4,43 106.4,127.4 "
	"156.8,186.5 10.6,12.4 25.1,29.4 32.2,37.8 14.8,17.4 22.4,24 "
	"31.8,27.6 14,5.4 23.2,5.9 36.5,2.1 10.6,-3.1 14.2,-4.9 22.2,-11.2 "
	"5.9,-4.5 62.7,-51 84.7,-69.1 5.6,-4.6 12.4,-10.3 15.2,-12.5 "
	"7.1,-5.8 41.8,-35 42.4,-35.6 0.3,-0.3 3.4,-3.1 7,-6.1 3.6,-3 "
	"6.7,-5.7 7,-6 0.3,-0.3 7,-6.1 15,-13 23.3,-20 72.9,-64.8 99,-89.4 "
	"31.2,-29.4 41.6,-39.2 56.8,-53.6 9.5,-9.1 23.3,-22.1 30.5,-28.9 "
	"14.5,-13.5 19.3,-19.3 24.2,-29.1 4.8,-9.5 6.7,-17.3 6.7,-26.5 "
	"0,-14.6 -2.5,-20.8 -17,-42.8 -5.9,-9 -13.2,-20.2 -16.2,-25 -3,-4.9 "
	"-7.3,-11.6 -9.5,-15 -2.2,-3.4 -9.6,-15 -16.4,-25.7 -6.8,-10.7 "
	"-15,-23.6 -18.1,-28.5 -3.2,-5 -9.4,-14.9 -13.8,-22.1 -11.4,-18.4 "
	"-19.3,-26.1 -32.9,-31.8 -9.7,-4.1 -10.2,-4.2 -51.3,-11.6 "
	"-18.4,-3.4 -41.8,-7.7 -52,-9.6 -10.2,-1.9 -22.3,-4.2 -27,-5 "
	"-15.4,-2.7 -41.4,-7.4 -46.5,-8.4 -18.4,-3.6 -98.5,-17.7 "
	"-159.5,-28.1 -16.2,-2.7 -33.3,-5.7 -38,-6.5 -4.7,-0.8 -11.2,-1.9 "
	"-14.5,-2.4 -3.3,-0.5 -10,-1.6 -15,-2.4 -14.2,-2.5 -35.6,-2 -48.5,1 "
	"z m 394,115.8 c 7.7,3.8 11.8,12.4 10.4,22 -1.5,11.6 -21,62.9 "
	"-58.1,153.6 -8,19.5 -18.5,45.2 -23.4,57 -7.6,18.8 -16.3,39.7 "
	"-31.1,75 -2.2,5.2 -5.1,12.2 -6.5,15.5 -1.4,3.3 -3.3,7.8 -4.3,10 "
	"-0.9,2.2 -3.7,8.5 -6,14 -13.2,31.3 -18.8,38 -32.7,39.2 -11.7,0.9 "
	"-17.1,-2 -42.8,-23.2 -14.9,-12.3 -44.9,-37.3 -57.1,-47.5 -5.5,-4.7 "
	"-16.5,-14 -24.5,-20.6 -12,-10.1 -14.9,-13.1 -17.3,-17.7 -5,-9.9 "
	"-3.3,-21.3 4.5,-30.1 3.3,-3.8 6.1,-6.1 41.4,-34.1 32.7,-25.9 "
	"37.4,-29.6 51.9,-41.1 59.2,-46.5 83.3,-66.5 102.1,-84.6 9.7,-9.3 "
	"11.6,-11.6 11.3,-13.7 -0.7,-6.3 -11.1,-2.9 -70.3,22.9 -73.4,32.1 "
	"-173.3,74.5 -196.2,83.3 -12.7,4.9 -18.7,6.2 -28.8,6.1 -10.7,-0.1 "
	"-17.1,-1.7 -34.3,-8.6 -17.3,-6.9 -44.6,-18.5 -50.5,-21.5 "
	"-11.2,-5.6 -17.6,-14.9 -16.3,-23.5 1.4,-8.7 7.8,-14.2 22.6,-19.5 "
	"11.9,-4.3 53.2,-17.1 72,-22.3 3.6,-1 11.9,-3.4 18.5,-5.3 15.4,-4.5 "
	"58.2,-16.2 70,-19.2 4.9,-1.3 14.2,-3.8 20.5,-5.5 6.3,-1.7 "
	"16.5,-4.5 22.5,-6 6.1,-1.6 16.9,-4.5 24,-6.5 14.7,-4 32.1,-8.6 "
	"57,-15.1 9.4,-2.4 24.9,-6.4 34.5,-9 9.6,-2.5 21.1,-5.4 25.5,-6.5 "
	"4.4,-1 11.4,-2.8 15.5,-3.9 19.4,-5 61,-14 75.5,-16.3 5.7,-0.9 "
	"15.9,0.5 20.5,2.7 z";

[[nodiscard]] QString Suffix() {
	return u".ton"_q;
}

[[nodiscard]] QString FullName(const QString &name) {
	const auto lower = name.trimmed().toLower();
	return lower.endsWith(Suffix()) ? lower : (lower + Suffix());
}

[[nodiscard]] QString ShortName(const QString &name) {
	const auto lower = name.trimmed().toLower();
	return lower.endsWith(Suffix())
		? lower.mid(0, lower.size() - Suffix().size())
		: lower;
}

[[nodiscard]] QSvgRenderer &Logo() {
	static const auto renderer = new QSvgRenderer(
		QByteArray(
			"<svg xmlns=\"http://www.w3.org/2000/svg\" "
			"viewBox=\"0 0 804.20 649.97\">"
			"<path fill=\"#FFFFFF\" fill-rule=\"evenodd\" d=\"")
		+ QByteArray(kLogoPath)
		+ QByteArray("\"/></svg>"));
	return *renderer;
}

// The TON symbol from its 56x56 logo (gramsrv metadata.tonMark), white
// with blue holes.
void PaintTonMark(QPainter &p, QPointF position, float64 size) {
	const auto k = size / 56.;
	const auto pt = [&](float64 x, float64 y) {
		return position + QPointF(x * k, y * k);
	};
	auto mark = QPainterPath();
	mark.moveTo(pt(37.5603, 15.6277));
	mark.lineTo(pt(18.4386, 15.6277));
	mark.cubicTo(pt(14.9228, 15.6277), pt(12.6944, 19.4202), pt(14.4632, 22.4861));
	mark.lineTo(pt(26.2644, 42.9409));
	mark.cubicTo(pt(27.0345, 44.2765), pt(28.9644, 44.2765), pt(29.7345, 42.9409));
	mark.lineTo(pt(41.5381, 22.4861));
	mark.cubicTo(pt(43.3045, 19.4251), pt(41.0761, 15.6277), pt(37.5627, 15.6277));
	mark.closeSubpath();
	p.fillPath(mark, QColor(255, 255, 255));

	auto holes = QPainterPath();
	holes.moveTo(pt(26.2524, 36.8092));
	holes.lineTo(pt(23.6847, 31.8327));
	holes.lineTo(pt(17.4833, 20.7414));
	holes.cubicTo(pt(17.0742, 20.0315), pt(17.5795, 19.1218), pt(18.4362, 19.1218));
	holes.lineTo(pt(26.2524, 19.1218));
	holes.closeSubpath();
	holes.moveTo(pt(29.7417, 36.8068));
	holes.lineTo(pt(32.3118, 31.8351));
	holes.lineTo(pt(38.5108, 20.739));
	holes.cubicTo(pt(38.9199, 20.0291), pt(38.4146, 19.1194), pt(37.5579, 19.1194));
	holes.lineTo(pt(29.7417, 19.1194));
	holes.closeSubpath();
	p.fillPath(holes, QColor::fromRgba(kTonBlue));
}

[[nodiscard]] QFont UnitsFont(const QFont &base, int size, int weight) {
	auto result = base;
	result.setPixelSize(size);
	result.setWeight(QFont::Weight(weight));
	result.setHintingPreference(QFont::PreferNoHinting);
	return result;
}

// Draws the card text centered on a line, like Android drawText with
// Align.CENTER at cy - (ascent + descent) / 2.
void DrawCentered(
		QPainter &p,
		const QFont &font,
		const QString &text,
		float64 cx,
		float64 cy) {
	const auto metrics = QFontMetricsF(font);
	const auto width = metrics.horizontalAdvance(text);
	const auto baseline = cy + (metrics.ascent() - metrics.descent()) / 2.;
	p.setFont(font);
	p.drawText(QPointF(cx - width / 2., baseline), text);
}

// The 1000-unit card painted into [0, 1000] x [0, 1000].
void PaintCardUnits(QPainter &p, const QString &name) {
	auto gradient = QLinearGradient(
		QPointF(0., 0.),
		QPointF(0.4 * kUnits / 0.52, 0.6 * kUnits / 0.52));
	gradient.setColorAt(0., QColor(0xF5, 0x55, 0x30));
	gradient.setColorAt(1., QColor(0xE2, 0x3E, 0x20));
	p.fillRect(QRectF(0., 0., kUnits, kUnits), gradient);

	// The app icon: a translucent white rounded square, the white mark.
	const auto icon = QRectF(
		(kUnits - kIconSize) / 2.,
		kIconY,
		kIconSize,
		kIconSize);
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(255, 255, 255, 0x2E));
	p.drawRoundedRect(icon, kIconRadius, kIconRadius);
	const auto box = kIconSize * 0.74;
	const auto logo = QSizeF(box, box / kLogoAspect);
	Logo().render(&p, QRectF(
		icon.center() - QPointF(logo.width() / 2., logo.height() / 2.),
		logo));

	// The TON disc in the icon's corner.
	const auto disc = QPointF(icon.right() - 14., icon.bottom() - 14.);
	const auto radius = 64.;
	p.setBrush(QColor(255, 255, 255));
	p.drawEllipse(disc, radius + 8., radius + 8.);
	p.setBrush(QColor::fromRgba(kTonBlue));
	p.drawEllipse(disc, radius, radius);
	PaintTonMark(p, disc - QPointF(radius, radius), 2. * radius);

	// The flat white pill with the name in the card's red, auto-fitted.
	const auto padMin = 40.;
	const auto maxText = kPillMaxWidth - 2. * padMin;
	auto size = kPillMaxEm;
	auto font = UnitsFont(st::semiboldFont->f, size, QFont::Bold);
	auto textWidth = QFontMetricsF(font).horizontalAdvance(name);
	while (textWidth > maxText && size > kPillMinEm) {
		size -= 2;
		font = UnitsFont(st::semiboldFont->f, size, QFont::Bold);
		textWidth = QFontMetricsF(font).horizontalAdvance(name);
	}
	auto label = name;
	if (textWidth > maxText) {
		label = QFontMetricsF(font).elidedText(name, Qt::ElideRight, maxText);
		textWidth = QFontMetricsF(font).horizontalAdvance(label);
	}
	const auto padX = std::max(padMin, float64(std::round(size * 0.62)));
	const auto pillHeight = float64(std::round(size * 1.62));
	const auto pillWidth = std::min(textWidth + 2. * padX, kPillMaxWidth);
	const auto pillTop = kPillCenterY - pillHeight / 2.;
	const auto pill = QRectF(
		(kUnits - pillWidth) / 2.,
		pillTop,
		pillWidth,
		pillHeight);
	p.setBrush(QColor(255, 255, 255));
	p.drawRoundedRect(pill, pillHeight / 2., pillHeight / 2.);
	p.setPen(QColor(0xDC, 0x38, 0x1A));
	DrawCentered(p, font, label, pill.center().x(), pill.center().y());

	// The ".ton домен StaticGram" line under the pill.
	const auto caption = QString::fromUtf8(
		".ton \xD0\xB4\xD0\xBE\xD0\xBC\xD0\xB5\xD0\xBD StaticGram");
	auto captionSize = 44;
	auto captionFont = UnitsFont(st::normalFont->f, captionSize, QFont::Medium);
	while (QFontMetricsF(captionFont).horizontalAdvance(caption)
			> kUnits - 140.
		&& captionSize > 28) {
		captionSize -= 2;
		captionFont = UnitsFont(st::normalFont->f, captionSize, QFont::Medium);
	}
	p.setPen(QColor(255, 255, 255, 0xBD));
	DrawCentered(
		p,
		captionFont,
		caption,
		kUnits / 2.,
		pillTop + pillHeight + kCaptionGap);
}

// An image of side x side physical pixels with the rounded corners.
template <typename Paint>
[[nodiscard]] QImage RenderRounded(int side, float64 radius, Paint &&paint) {
	auto result = QImage(side, side, QImage::Format_ARGB32_Premultiplied);
	result.fill(Qt::transparent);
	auto p = QPainter(&result);
	auto hq = PainterHighQualityEnabler(p);
	if (radius > 0.) {
		auto clip = QPainterPath();
		clip.addRoundedRect(
			QRectF(0., 0., side, side),
			side * radius,
			side * radius);
		p.setClipPath(clip);
	}
	paint(p);
	p.end();
	return result;
}

// The server renders of minted domains by the short name, in memory.
class RemoteCards final {
public:
	[[nodiscard]] const QImage *image(const QString &shortName) {
		const auto i = _ready.constFind(shortName);
		if (i != _ready.cend()) {
			return &i.value();
		}
		load(shortName);
		return nullptr;
	}
	[[nodiscard]] rpl::producer<QString> loaded() const {
		return _loaded.events();
	}

private:
	void load(const QString &shortName) {
		if (shortName.isEmpty() || _loading.contains(shortName)) {
			return;
		}
		const auto failed = _failed.constFind(shortName);
		if (failed != _failed.cend()
			&& crl::now() - failed.value() < kRetryAfter) {
			return;
		}
		const auto app = QCoreApplication::instance();
		if (!app) {
			return;
		}
		if (!_network) {
			_network = new QNetworkAccessManager(app);
		}
		_loading.insert(shortName);
		auto request = QNetworkRequest(QUrl(CardMediaUrl(shortName)));
		request.setTransferTimeout(30'000);
		const auto reply = _network->get(request);
		QObject::connect(reply, &QNetworkReply::finished, _network, [=] {
			reply->deleteLater();
			const auto status = reply->attribute(
				QNetworkRequest::HttpStatusCodeAttribute).toInt();
			if (reply->error() != QNetworkReply::NoError || status != 200) {
				fail(shortName);
				return;
			}
			const auto bytes = reply->read(kMaxDownload + 1);
			if (bytes.isEmpty() || bytes.size() > kMaxDownload) {
				fail(shortName);
				return;
			}
			crl::async([=] {
				auto image = QImage::fromData(bytes);
				if (!image.isNull()) {
					image = image.scaled(
						kRemoteSide,
						kRemoteSide,
						Qt::IgnoreAspectRatio,
						Qt::SmoothTransformation
					).convertToFormat(QImage::Format_ARGB32_Premultiplied);
				}
				crl::on_main([=, image = std::move(image)]() mutable {
					if (image.isNull()) {
						fail(shortName);
					} else {
						finish(shortName, std::move(image));
					}
				});
			});
		});
	}
	void fail(const QString &shortName) {
		_loading.remove(shortName);
		_failed.insert(shortName, crl::now());
	}
	void finish(const QString &shortName, QImage image) {
		_loading.remove(shortName);
		_failed.remove(shortName);
		if (_ready.size() >= kMaxCached) {
			_ready.clear();
		}
		_ready.insert(shortName, std::move(image));
		_loaded.fire_copy(shortName);
	}

	QNetworkAccessManager *_network = nullptr;
	QHash<QString, QImage> _ready;
	QSet<QString> _loading;
	QHash<QString, crl::time> _failed;
	rpl::event_stream<QString> _loaded;

};

[[nodiscard]] RemoteCards &Remote() {
	static const auto instance = new RemoteCards();
	return *instance;
}

} // namespace

QString CardMediaUrl(const QString &name) {
	return u"https://fragment.staticgram.top/api/nft/item/domain/"_q
		+ QString::fromLatin1(QUrl::toPercentEncoding(ShortName(name)))
		+ u"/media.png?v=v3"_q;
}

CardView::CardView(
	QWidget *parent,
	const QString &name,
	float64 radiusFraction,
	bool server)
: RpWidget(parent)
, _radius(radiusFraction)
, _server(server) {
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setName(name);
	Remote().loaded() | rpl::filter([=](const QString &shortName) {
		return _server && (shortName == _short);
	}) | rpl::to_empty | rpl::on_next([=] {
		_remote = QImage();
		update();
	}, lifetime());
}

void CardView::setName(const QString &name) {
	const auto full = FullName(name);
	if (full == _name) {
		return;
	}
	_name = full;
	_short = ShortName(name);
	_local = QImage();
	_remote = QImage();
	update();
}

void CardView::setServer(bool server) {
	if (_server == server) {
		return;
	}
	_server = server;
	_remote = QImage();
	update();
}

QRect CardView::cardRect() const {
	const auto side = std::min(width(), height());
	return QRect(
		(width() - side) / 2,
		(height() - side) / 2,
		side,
		side);
}

void CardView::paintEvent(QPaintEvent *e) {
	const auto rect = cardRect();
	if (rect.isEmpty()) {
		return;
	}
	const auto ratio = style::DevicePixelRatio();
	const auto side = rect.width() * ratio;
	auto p = QPainter(this);
	if (_server) {
		if (const auto image = Remote().image(_short)) {
			if (_remote.width() != side) {
				_remote = RenderRounded(side, _radius, [&](QPainter &q) {
					q.drawImage(QRect(0, 0, side, side), *image);
				});
				_remote.setDevicePixelRatio(ratio);
			}
			p.drawImage(rect.topLeft(), _remote);
			return;
		}
	}
	if (_local.width() != side) {
		_local = RenderRounded(side, _radius, [&](QPainter &q) {
			q.scale(side / kUnits, side / kUnits);
			PaintCardUnits(q, _name);
		});
		_local.setDevicePixelRatio(ratio);
	}
	p.drawImage(rect.topLeft(), _local);
}

} // namespace StaticGram::Domains
