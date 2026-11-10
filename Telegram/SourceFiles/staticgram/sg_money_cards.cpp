/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_money_cards.h"

#include "chat_helpers/stickers_lottie.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "data/data_star_gift.h"
#include "data/data_user.h"
#include "data/stickers/data_custom_emoji.h"
#include "history/view/media/history_view_sticker_player.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/painter.h"
#include "ui/text/text_custom_emoji.h"
#include "ui/top_background_gradient.h"
#include "styles/style_layers.h"
#include "styles/style_staticgram.h"

#include <QtWidgets/QApplication>

#include <QtGui/QPainterPath>

namespace StaticGram::Money {
namespace {

constexpr auto kCardRatio = 0.6;
constexpr auto kSwipeThreshold = 0.15;
constexpr auto kOverscroll = 0.2;
constexpr auto kAnimationDuration = crl::time(280);
constexpr auto kCardsCount = 2;

struct StockLook {
	QColor center;
	QColor edge;
};

[[nodiscard]] StockLook DefaultLook(Kind kind) {
	// Android WalletCardComponent.defaultBackdrop: gold for Stars, blue
	// for GRAM.
	return (kind == Kind::Stars)
		? StockLook{ QColor(0xF2, 0xB3, 0x3D), QColor(0xD9, 0x77, 0x1F) }
		: StockLook{ QColor(0x4A, 0x9B, 0xFF), QColor(0x1F, 0x5B, 0xD6) };
}

[[nodiscard]] QColor WithAlpha(QColor color, float64 alpha) {
	color.setAlphaF(color.alphaF() * alpha);
	return color;
}

} // namespace

int CardHeight(int width) {
	return int(std::round(width * kCardRatio));
}

int IndexByKind(Kind kind) {
	return (kind == Kind::Stars) ? 1 : 0;
}

Kind KindByIndex(int index) {
	return (index == 1) ? Kind::Stars : Kind::Gram;
}

struct CardFace::Sticker {
	CardSticker info;
	std::shared_ptr<Data::DocumentMedia> media;
	std::unique_ptr<HistoryView::StickerPlayer> player;
	rpl::lifetime lifetime;
};

CardFace::CardFace(
	not_null<UserData*> user,
	Kind kind,
	Fn<void()> repaint)
: _user(user)
, _kind(kind)
, _repaint(std::move(repaint)) {
}

CardFace::~CardFace() = default;

Kind CardFace::kind() const {
	return _kind;
}

int64 CardFace::balance() const {
	return _balance;
}

void CardFace::setBalance(int64 balance) {
	if (_balance != balance) {
		_balance = balance;
		_repaint();
	}
}

void CardFace::setCard(const Card *card) {
	const auto background = card ? card->background : nullptr;
	if (_background != background) {
		_background = background;
		_gradient = QImage();
		_patternCache.clear();
		_pattern = _background
			? _user->owner().customEmojiManager().create(
				_background->pattern.document,
				_repaint,
				Data::CustomEmojiSizeTag::Large)
			: nullptr;
	}
	if (card) {
		_balance = card->balance;
	}
	static const auto kEmpty = std::vector<CardSticker>();
	const auto &now = card ? card->stickers : kEmpty;
	const auto sameDocuments = [&] {
		if (now.size() != _stickers.size()) {
			return false;
		}
		for (auto i = 0, count = int(now.size()); i != count; ++i) {
			if (now[i].document != _stickers[i]->info.document) {
				return false;
			}
		}
		return true;
	}();
	if (sameDocuments) {
		// Only the placement changed (the editor moves them): keep the
		// loaded players.
		for (auto i = 0, count = int(now.size()); i != count; ++i) {
			_stickers[i]->info = now[i];
		}
		_repaint();
		return;
	}
	auto was = base::take(_stickers);
	for (const auto &info : now) {
		const auto i = ranges::find_if(was, [&](const auto &sticker) {
			return sticker && (sticker->info.document == info.document);
		});
		if (i != end(was)) {
			auto reused = std::move(*i);
			reused->info = info;
			_stickers.push_back(std::move(reused));
		} else {
			_stickers.push_back(std::make_unique<Sticker>(Sticker{
				.info = info,
			}));
			loadSticker(_stickers.back().get());
		}
	}
	_repaint();
}

void CardFace::loadSticker(not_null<Sticker*> sticker) {
	const auto document = sticker->info.document;
	const auto media = document->createMediaView();
	sticker->media = media;
	if (document->sticker()) {
		media->checkStickerLarge();
	} else {
		document->forceToCache(true);
		document->save(Data::FileOrigin(), QString());
	}
	const auto create = [=] {
		const auto width = _lastWidth
			? _lastWidth
			: st::boxWideWidth;
		const auto side = std::max(
			int(std::round(sticker->info.scale * width)),
			8);
		const auto size = QSize(side, side);
		auto result = std::unique_ptr<HistoryView::StickerPlayer>();
		const auto info = document->sticker();
		if (info && info->isLottie()) {
			result = std::make_unique<HistoryView::LottiePlayer>(
				ChatHelpers::LottiePlayerFromDocument(
					media.get(),
					ChatHelpers::StickerLottieSize::InlineResults,
					size * style::DevicePixelRatio(),
					Lottie::Quality::High));
		} else if (info && info->isWebm()) {
			result = std::make_unique<HistoryView::WebmPlayer>(
				document->location(),
				media->bytes(),
				size);
		} else {
			result = std::make_unique<HistoryView::StaticStickerPlayer>(
				document->location(),
				media->bytes(),
				size);
		}
		result->setRepaintCallback(_repaint);
		sticker->player = std::move(result);
		_repaint();
	};
	rpl::single(rpl::empty) | rpl::then(
		document->session().downloaderTaskFinished()
	) | rpl::filter([=] {
		return media->loaded();
	}) | rpl::take(1) | rpl::on_next(create, sticker->lifetime);
}

void CardFace::paint(QPainter &p, QRect rect, bool active) {
	_lastWidth = rect.width();
	const auto radius = st::sgMoneyCardRadius;
	p.save();
	{
		auto hq = PainterHighQualityEnabler(p);
		auto path = QPainterPath();
		path.addRoundedRect(rect, radius, radius);
		p.setClipPath(path);
	}
	paintBackground(p, rect);
	paintStickers(p, rect, active);
	paintTexts(p, rect);
	p.restore();
}

void CardFace::paintBackground(QPainter &p, QRect rect) {
	const auto ratio = style::DevicePixelRatio();
	if (_gradient.size() != rect.size() * ratio) {
		if (_background) {
			_gradient = Ui::CreateTopBgGradient(
				rect.size(),
				_background->backdrop);
		} else {
			const auto look = DefaultLook(_kind);
			_gradient = Ui::CreateTopBgGradient(
				rect.size(),
				look.center,
				look.edge,
				false);
		}
	}
	p.drawImage(rect.topLeft(), _gradient);
	if (_background && _pattern) {
		Ui::PaintBgPoints(
			p,
			Ui::PatternBgPoints(),
			_patternCache,
			_pattern.get(),
			*_background,
			rect,
			1.);
	}
}

void CardFace::paintStickers(QPainter &p, QRect rect, bool active) {
	const auto now = crl::now();
	for (const auto &sticker : _stickers) {
		const auto player = sticker->player.get();
		if (!player || !player->ready()) {
			continue;
		}
		const auto &info = sticker->info;
		const auto side = std::max(
			int(std::round(info.scale * rect.width())),
			8);
		const auto frame = player->frame(
			QSize(side, side),
			QColor(0, 0, 0, 0),
			false,
			now,
			!active);
		if (frame.image.isNull()) {
			continue;
		}
		const auto imageRatio = frame.image.devicePixelRatio();
		const auto natural = QSizeF(frame.image.size()) / imageRatio;
		const auto target = natural.scaled(
			QSizeF(side, side),
			Qt::KeepAspectRatio);
		const auto center = QPointF(
			rect.x() + info.x * rect.width(),
			rect.y() + info.y * rect.height());
		p.save();
		{
			auto hq = PainterHighQualityEnabler(p);
			p.translate(center);
			p.rotate(info.rotation);
			p.drawImage(
				QRectF(
					-target.width() / 2.,
					-target.height() / 2.,
					target.width(),
					target.height()),
				frame.image);
		}
		p.restore();
		if (active) {
			player->markFrameShown();
		}
	}
}

void CardFace::paintTexts(QPainter &p, QRect rect) {
	const auto inner = rect.marginsRemoved(st::sgMoneyCardInner);
	const auto color = _background
		? _background->backdrop.textColor
		: QColor(255, 255, 255);
	const auto sub = WithAlpha(color, 0.72);
	const auto session = &_user->session();

	const auto &labelFont = st::sgMoneyCardLabelFont;
	p.setFont(labelFont);
	p.setPen(sub);
	p.drawText(
		inner.x(),
		inner.y() + labelFont->ascent,
		(_kind == Kind::Stars)
			? tr::lng_sg_money_card_stars(tr::now)
			: tr::lng_sg_money_card_gram(tr::now));

	const auto &balanceFont = st::sgMoneyCardBalanceFont;
	const auto &unitFont = st::sgMoneyCardUnitFont;
	const auto amount = FormatAmount(_kind, _balance);
	const auto unit = u" "_q + ((_kind == Kind::Stars)
		? tr::lng_sg_money_unit_stars(tr::now)
		: tr::lng_sg_money_unit_gram(tr::now));
	const auto balanceTop = inner.y()
		+ labelFont->height
		+ st::sgMoneyCardBalanceTop
		- balanceFont->height / 2;
	const auto baseline = balanceTop + balanceFont->ascent;
	p.setFont(balanceFont);
	p.setPen(color);
	p.drawText(inner.x(), baseline, amount);
	p.setFont(unitFont);
	p.setPen(sub);
	p.drawText(inner.x() + balanceFont->width(amount), baseline, unit);

	const auto fiat = FiatText(session, _kind, _balance);
	const auto &fiatFont = st::sgMoneyCardFiatFont;
	if (!fiat.isEmpty()) {
		p.setFont(fiatFont);
		p.setPen(sub);
		p.drawText(
			inner.x(),
			(balanceTop
				+ balanceFont->height
				+ st::sgMoneyCardFiatSkip
				+ fiatFont->ascent),
			fiat);
	}

	const auto &numberFont = st::sgMoneyCardNumberFont;
	const auto &holderFont = st::sgMoneyCardHolderFont;
	const auto numberBaseline = inner.y()
		+ inner.height()
		- numberFont->descent;
	p.setFont(numberFont);
	p.setPen(color);
	p.drawText(
		inner.x(),
		numberBaseline,
		numberFont->elided(CardNumber(_kind, _user), inner.width()));
	const auto holderBaseline = numberBaseline
		- numberFont->ascent
		- st::sgMoneyCardNumberSkip
		- holderFont->descent;
	p.setFont(holderFont);
	p.setPen(sub);
	p.drawText(
		inner.x(),
		holderBaseline,
		holderFont->elided(_user->name().toUpper(), inner.width()));
}

CardsPager::CardsPager(
	QWidget *parent,
	not_null<UserData*> user,
	Kind initial)
: RpWidget(parent)
, _index(IndexByKind(initial))
, _settled(IndexByKind(initial))
, _position(IndexByKind(initial)) {
	const auto repaint = [=] { update(); };
	_faces[0] = std::make_unique<CardFace>(user, Kind::Gram, repaint);
	_faces[1] = std::make_unique<CardFace>(user, Kind::Stars, repaint);
	setCursor(style::cur_pointer);

	// Repaint the rouble values once the exchange rate arrives.
	FiatValue(
		&user->session(),
		Kind::Gram,
		rpl::single(kNanosInOne)
	) | rpl::skip(1) | rpl::on_next(repaint, lifetime());
}

CardsPager::~CardsPager() = default;

void CardsPager::setCards(const Cards &cards) {
	for (const auto &face : _faces) {
		face->setCard(cards.find(face->kind()));
	}
}

void CardsPager::setBalance(Kind kind, int64 balance) {
	_faces[IndexByKind(kind)]->setBalance(balance);
}

int64 CardsPager::balance(Kind kind) const {
	return _faces[IndexByKind(kind)]->balance();
}

void CardsPager::showKind(Kind kind) {
	animateTo(IndexByKind(kind));
}

Kind CardsPager::kind() const {
	return KindByIndex(_index.current());
}

rpl::producer<Kind> CardsPager::kindValue() const {
	return _index.value() | rpl::map(KindByIndex);
}

rpl::producer<Kind> CardsPager::settledKindValue() const {
	return _settled.value() | rpl::map(KindByIndex);
}

int CardsPager::cardWidth() const {
	const auto &padding = st::sgMoneyCardsPadding;
	return std::max(width() - padding.left() - padding.right(), 1);
}

int CardsPager::cardHeight() const {
	return int(std::round(cardWidth() * kCardRatio));
}

int CardsPager::resizeGetHeight(int newWidth) {
	const auto &padding = st::sgMoneyCardsPadding;
	const auto card = std::max(newWidth - padding.left() - padding.right(), 1);
	return padding.top()
		+ int(std::round(card * kCardRatio))
		+ st::sgMoneyDotsHeight;
}

QRect CardsPager::cardRect(int index, float64 position) const {
	const auto &padding = st::sgMoneyCardsPadding;
	const auto step = cardWidth() + st::sgMoneyCardSkip;
	return QRect(
		padding.left() + int(std::round((index - position) * step)),
		padding.top(),
		cardWidth(),
		cardHeight());
}

QRect CardsPager::dotsRect() const {
	const auto top = st::sgMoneyCardsPadding.top() + cardHeight();
	return QRect(0, top, width(), st::sgMoneyDotsHeight);
}

int CardsPager::indexAt(QPoint point) const {
	for (auto i = 0; i != kCardsCount; ++i) {
		if (cardRect(i, _position).contains(point)) {
			return i;
		}
	}
	const auto dots = dotsRect();
	if (dots.contains(point)) {
		return (point.x() < dots.center().x()) ? 0 : 1;
	}
	return -1;
}

void CardsPager::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	// StaticGram: a clipped pager, the neighbour card does not peek out
	// at the box edge and slides in only inside the card slot.
	const auto &padding = st::sgMoneyCardsPadding;
	const auto visible = QRect(
		padding.left(),
		0,
		cardWidth(),
		padding.top() + cardHeight());
	p.setClipRect(visible);
	for (auto i = 0; i != kCardsCount; ++i) {
		const auto card = cardRect(i, _position);
		if (card.intersects(visible)) {
			_faces[i]->paint(p, card, i == _index.current());
		}
	}
	p.setClipping(false);

	auto hq = PainterHighQualityEnabler(p);
	const auto dots = dotsRect();
	const auto size = st::sgMoneyDotSize;
	const auto skip = st::sgMoneyDotSkip;
	const auto full = kCardsCount * size + (kCardsCount - 1) * skip;
	auto x = (width() - full) / 2;
	const auto y = dots.y() + (dots.height() - size) / 2;
	p.setPen(Qt::NoPen);
	for (auto i = 0; i != kCardsCount; ++i) {
		const auto shown = std::clamp(
			1. - std::abs(_position - i),
			0.,
			1.);
		auto color = st::windowSubTextFg->c;
		color.setAlphaF(0.3 + 0.7 * shown);
		p.setBrush(color);
		p.drawEllipse(x, y, size, size);
		x += size + skip;
	}
}

void CardsPager::mousePressEvent(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) {
		return;
	}
	_animation.stop();
	_pressed = e->pos();
	_pressedPosition = _position;
	_dragging = false;
}

void CardsPager::mouseMoveEvent(QMouseEvent *e) {
	if (!_pressed) {
		return;
	}
	const auto dx = e->pos().x() - _pressed->x();
	if (!_dragging && std::abs(dx) >= QApplication::startDragDistance()) {
		_dragging = true;
	}
	if (_dragging) {
		const auto step = float64(cardWidth() + st::sgMoneyCardSkip);
		_position = std::clamp(
			_pressedPosition - dx / step,
			-kOverscroll,
			(kCardsCount - 1) + kOverscroll);
		update();
	}
}

void CardsPager::mouseReleaseEvent(QMouseEvent *e) {
	if (!_pressed) {
		return;
	}
	const auto pressed = *base::take(_pressed);
	if (_dragging) {
		_dragging = false;
		const auto from = int(std::round(_pressedPosition));
		const auto delta = _position - from;
		const auto target = (delta > kSwipeThreshold)
			? (from + 1)
			: (delta < -kSwipeThreshold)
			? (from - 1)
			: from;
		animateTo(std::clamp(target, 0, kCardsCount - 1));
		return;
	}
	const auto index = indexAt(e->pos());
	if (index >= 0 && index == indexAt(pressed)) {
		animateTo(index);
	}
}

void CardsPager::wheelEvent(QWheelEvent *e) {
	const auto delta = e->angleDelta();
	if (std::abs(delta.x()) <= std::abs(delta.y())
		|| std::abs(delta.x()) < 30
		|| _animation.animating()) {
		e->ignore();
		return;
	}
	animateTo(std::clamp(
		_index.current() + ((delta.x() < 0) ? 1 : -1),
		0,
		kCardsCount - 1));
}

void CardsPager::animateTo(int index) {
	index = std::clamp(index, 0, kCardsCount - 1);
	const auto from = _position;
	_index = index;
	if (std::abs(from - index) < 0.001) {
		_position = index;
		_settled = index;
		update();
		return;
	}
	_animation.start([=] {
		_position = _animation.value(index);
		if (!_animation.animating()) {
			_settled = index;
		}
		update();
	}, from, index, kAnimationDuration, anim::easeOutCubic);
}

} // namespace StaticGram::Money
