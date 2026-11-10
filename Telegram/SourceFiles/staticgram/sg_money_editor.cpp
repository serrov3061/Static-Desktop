/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_money_editor.h"

#include "apiwrap.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_session.h"
#include "data/data_star_gift.h"
#include "data/data_user.h"
#include "data/stickers/data_stickers.h"
#include "info/peer_gifts/info_peer_gifts_common.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/session/session_show.h"
#include "mokagram/ui/mokagram_box_parts.h"
#include "staticgram/sg_box_parts.h"
#include "staticgram/sg_money_cards.h"
#include "ui/image/image.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/discrete_sliders.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "styles/style_staticgram.h"
#include "styles/style_widgets.h"

#include <QtWidgets/QApplication>

namespace StaticGram::Money {
namespace {

constexpr auto kDefaultScale = 0.231; // Android SGCardStickers.
constexpr auto kMinScale = 0.05;
constexpr auto kMaxScale = 1.;
constexpr auto kGiftsPerRow = 3;
constexpr auto kGiftsLimit = 300;
constexpr auto kStickersLimit = 400;
constexpr auto kWheelScaleStep = 1.08;
constexpr auto kWheelRotateStep = 5.;

[[nodiscard]] float64 NormalizeRotation(float64 value) {
	while (value > 180.) {
		value -= 360.;
	}
	while (value < -180.) {
		value += 360.;
	}
	return value;
}

[[nodiscard]] bool SameSticker(
		const CardSticker &a,
		const QString &slug,
		DocumentData *document) {
	return !slug.isEmpty()
		? (a.giftSlug == slug)
		: (a.giftSlug.isEmpty() && a.document.get() == document);
}

// Android SGCardStickers.isStaticSticker: the server takes static
// webp / png stickers only.
[[nodiscard]] bool IsStaticSticker(not_null<DocumentData*> document) {
	const auto sticker = document->sticker();
	if (!sticker || sticker->isAnimated()) {
		return false;
	}
	const auto mime = document->mimeString();
	return (mime == u"image/webp"_q) || (mime == u"image/png"_q);
}

class EditorCard final : public Ui::RpWidget {
public:
	EditorCard(QWidget *parent, not_null<UserData*> user, Card card);

	[[nodiscard]] const Card &card() const;
	void setBackground(std::shared_ptr<Data::UniqueGift> gift);
	[[nodiscard]] bool hasSticker(
		const QString &slug,
		DocumentData *document) const;
	// Adds or removes the sticker, false when there is no room left.
	bool toggleSticker(CardSticker sticker);
	void removeSelected();
	void bringSelectedToFront();

	[[nodiscard]] rpl::producer<bool> hasSelectionValue() const;
	[[nodiscard]] rpl::producer<> changes() const;

protected:
	int resizeGetHeight(int newWidth) override;
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void wheelEvent(QWheelEvent *e) override;

private:
	enum class Drag {
		None,
		Move,
		Handle,
	};

	[[nodiscard]] QRect cardRect() const;
	[[nodiscard]] float64 stickerSide(const CardSticker &sticker) const;
	[[nodiscard]] QPointF stickerCenter(const CardSticker &sticker) const;
	[[nodiscard]] QPointF handlePoint(const CardSticker &sticker) const;
	[[nodiscard]] bool stickerContains(
		const CardSticker &sticker,
		QPointF point) const;
	[[nodiscard]] int stickerAt(QPointF point) const;
	[[nodiscard]] bool handleContains(QPointF point) const;
	[[nodiscard]] CardSticker *selected();
	void select(int index);
	void applyChange();
	void updateCursor(QPointF point);

	CardFace _face;
	Card _card;
	rpl::variable<int> _selected = -1;
	rpl::event_stream<> _changes;

	Drag _drag = Drag::None;
	QPointF _grab;
	float64 _grabScale = 0.;
	float64 _grabRotation = 0.;
	float64 _grabAngle = 0.;
	float64 _grabDistance = 0.;

};

EditorCard::EditorCard(QWidget *parent, not_null<UserData*> user, Card card)
: RpWidget(parent)
, _face(user, card.kind, [=] { update(); })
, _card(std::move(card)) {
	_face.setCard(&_card);
	setMouseTracking(true);
}

const Card &EditorCard::card() const {
	return _card;
}

void EditorCard::setBackground(std::shared_ptr<Data::UniqueGift> gift) {
	_card.background = std::move(gift);
	applyChange();
}

bool EditorCard::hasSticker(
		const QString &slug,
		DocumentData *document) const {
	return ranges::any_of(_card.stickers, [&](const CardSticker &s) {
		return SameSticker(s, slug, document);
	});
}

bool EditorCard::toggleSticker(CardSticker sticker) {
	const auto document = sticker.document.get();
	const auto i = ranges::find_if(_card.stickers, [&](
			const CardSticker &s) {
		return SameSticker(s, sticker.giftSlug, document);
	});
	if (i != end(_card.stickers)) {
		_card.stickers.erase(i);
		_selected = -1;
		applyChange();
		return true;
	} else if (int(_card.stickers.size()) >= kMaxStickers) {
		return false;
	}
	// A spread of free spots instead of Android's toss animation.
	const auto n = int(_card.stickers.size());
	sticker.x = 0.22 + 0.56 * ((n * 37) % 100) / 100.;
	sticker.y = 0.3 + 0.4 * ((n * 61) % 100) / 100.;
	sticker.scale = kDefaultScale;
	sticker.rotation = 0.;
	_card.stickers.push_back(std::move(sticker));
	_selected = int(_card.stickers.size()) - 1;
	applyChange();
	return true;
}

void EditorCard::removeSelected() {
	const auto index = _selected.current();
	if (index >= 0 && index < int(_card.stickers.size())) {
		_card.stickers.erase(begin(_card.stickers) + index);
		_selected = -1;
		applyChange();
	}
}

void EditorCard::bringSelectedToFront() {
	const auto index = _selected.current();
	const auto count = int(_card.stickers.size());
	if (index >= 0 && index + 1 < count) {
		auto moved = std::move(_card.stickers[index]);
		_card.stickers.erase(begin(_card.stickers) + index);
		_card.stickers.push_back(std::move(moved));
		_selected = count - 1;
		applyChange();
	}
}

rpl::producer<bool> EditorCard::hasSelectionValue() const {
	return _selected.value() | rpl::map(rpl::mappers::_1 >= 0);
}

rpl::producer<> EditorCard::changes() const {
	return _changes.events();
}

void EditorCard::applyChange() {
	_face.setCard(&_card);
	_changes.fire({});
	update();
}

CardSticker *EditorCard::selected() {
	const auto index = _selected.current();
	return (index >= 0 && index < int(_card.stickers.size()))
		? &_card.stickers[index]
		: nullptr;
}

void EditorCard::select(int index) {
	_selected = index;
	update();
}

QRect EditorCard::cardRect() const {
	const auto &padding = st::sgCardEditorCardPadding;
	const auto width = std::max(
		this->width() - padding.left() - padding.right(),
		1);
	return QRect(padding.left(), padding.top(), width, CardHeight(width));
}

int EditorCard::resizeGetHeight(int newWidth) {
	const auto &padding = st::sgCardEditorCardPadding;
	const auto width = std::max(
		newWidth - padding.left() - padding.right(),
		1);
	return padding.top() + CardHeight(width) + padding.bottom();
}

float64 EditorCard::stickerSide(const CardSticker &sticker) const {
	return std::max(sticker.scale * cardRect().width(), 8.);
}

QPointF EditorCard::stickerCenter(const CardSticker &sticker) const {
	const auto rect = cardRect();
	return QPointF(
		rect.x() + sticker.x * rect.width(),
		rect.y() + sticker.y * rect.height());
}

QPointF EditorCard::handlePoint(const CardSticker &sticker) const {
	const auto half = stickerSide(sticker) / 2.;
	const auto angle = sticker.rotation * M_PI / 180.;
	const auto dx = half * std::cos(angle) - half * std::sin(angle);
	const auto dy = half * std::sin(angle) + half * std::cos(angle);
	return stickerCenter(sticker) + QPointF(dx, dy);
}

bool EditorCard::stickerContains(
		const CardSticker &sticker,
		QPointF point) const {
	const auto delta = point - stickerCenter(sticker);
	const auto angle = -sticker.rotation * M_PI / 180.;
	const auto x = delta.x() * std::cos(angle) - delta.y() * std::sin(angle);
	const auto y = delta.x() * std::sin(angle) + delta.y() * std::cos(angle);
	const auto half = stickerSide(sticker) / 2.;
	return std::abs(x) <= half && std::abs(y) <= half;
}

int EditorCard::stickerAt(QPointF point) const {
	for (auto i = int(_card.stickers.size()); i != 0;) {
		if (stickerContains(_card.stickers[--i], point)) {
			return i;
		}
	}
	return -1;
}

bool EditorCard::handleContains(QPointF point) const {
	const auto index = _selected.current();
	if (index < 0 || index >= int(_card.stickers.size())) {
		return false;
	}
	const auto delta = point - handlePoint(_card.stickers[index]);
	const auto radius = st::sgCardEditorHandleSize;
	return (delta.x() * delta.x() + delta.y() * delta.y())
		<= float64(radius * radius);
}

void EditorCard::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	_face.paint(p, cardRect(), true);

	const auto index = _selected.current();
	if (index < 0 || index >= int(_card.stickers.size())) {
		return;
	}
	const auto &sticker = _card.stickers[index];
	const auto half = stickerSide(sticker) / 2.;
	auto hq = PainterHighQualityEnabler(p);
	p.save();
	p.translate(stickerCenter(sticker));
	p.rotate(sticker.rotation);
	auto pen = QPen(QColor(255, 255, 255, 230));
	pen.setWidthF(style::ConvertScaleExact(1.5));
	pen.setStyle(Qt::DashLine);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	p.drawRect(QRectF(-half, -half, half * 2., half * 2.));
	p.restore();

	const auto handle = handlePoint(sticker);
	const auto size = float64(st::sgCardEditorHandleSize);
	p.setPen(QPen(st::windowBgActive->c, style::ConvertScaleExact(2.)));
	p.setBrush(QColor(255, 255, 255));
	p.drawEllipse(handle, size / 2., size / 2.);
}

void EditorCard::updateCursor(QPointF point) {
	setCursor(handleContains(point)
		? style::cur_sizefdiag
		: (stickerAt(point) >= 0)
		? style::cur_sizeall
		: style::cur_default);
}

void EditorCard::mousePressEvent(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) {
		return;
	}
	const auto point = QPointF(e->pos());
	if (handleContains(point)) {
		const auto sticker = selected();
		const auto delta = point - stickerCenter(*sticker);
		_drag = Drag::Handle;
		_grabScale = sticker->scale;
		_grabRotation = sticker->rotation;
		_grabAngle = std::atan2(delta.y(), delta.x()) * 180. / M_PI;
		_grabDistance = std::max(std::hypot(delta.x(), delta.y()), 1.);
		return;
	}
	const auto index = stickerAt(point);
	select(index);
	if (index >= 0) {
		_drag = Drag::Move;
		_grab = point - stickerCenter(_card.stickers[index]);
	}
}

void EditorCard::mouseMoveEvent(QMouseEvent *e) {
	const auto point = QPointF(e->pos());
	const auto sticker = selected();
	if (_drag == Drag::None || !sticker) {
		updateCursor(point);
		return;
	}
	const auto rect = cardRect();
	if (_drag == Drag::Move) {
		const auto center = point - _grab;
		sticker->x = std::clamp(
			(center.x() - rect.x()) / float64(rect.width()),
			-0.3,
			1.3);
		sticker->y = std::clamp(
			(center.y() - rect.y()) / float64(rect.height()),
			-0.3,
			1.3);
	} else {
		const auto delta = point - stickerCenter(*sticker);
		const auto distance = std::hypot(delta.x(), delta.y());
		const auto angle = std::atan2(delta.y(), delta.x()) * 180. / M_PI;
		sticker->scale = std::clamp(
			_grabScale * distance / _grabDistance,
			kMinScale,
			kMaxScale);
		sticker->rotation = NormalizeRotation(
			_grabRotation + angle - _grabAngle);
	}
	applyChange();
}

void EditorCard::mouseReleaseEvent(QMouseEvent *e) {
	const auto drag = std::exchange(_drag, Drag::None);
	const auto sticker = selected();
	if (drag != Drag::Move || !sticker) {
		return;
	}
	// Android: a sticker dropped off the card is removed.
	if (sticker->x < 0. || sticker->x > 1. || sticker->y < 0. || sticker->y > 1.) {
		removeSelected();
	}
	updateCursor(QPointF(e->pos()));
}

void EditorCard::wheelEvent(QWheelEvent *e) {
	const auto sticker = selected();
	const auto delta = e->angleDelta().y();
	if (!sticker || !delta) {
		e->ignore();
		return;
	}
	if (e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) {
		sticker->rotation = NormalizeRotation(
			sticker->rotation + ((delta > 0) ? 1 : -1) * kWheelRotateStep);
	} else {
		sticker->scale = std::clamp(
			sticker->scale * ((delta > 0)
				? kWheelScaleStep
				: (1. / kWheelScaleStep)),
			kMinScale,
			kMaxScale);
	}
	applyChange();
}

// Own unique gifts as GiftButton cells (Android GiftCellView).
class GiftPicker final : public Ui::RpWidget {
public:
	GiftPicker(
		QWidget *parent,
		not_null<Main::Session*> session,
		const std::vector<Data::StarGift> &gifts,
		Fn<void(const Data::StarGift&)> clicked,
		Fn<bool(const Data::StarGift&)> selected);

	void refreshSelection();

protected:
	int resizeGetHeight(int newWidth) override;

private:
	struct Entry {
		Data::StarGift gift;
		std::unique_ptr<Info::PeerGifts::GiftButton> button;
	};

	const Fn<bool(const Data::StarGift&)> _selected;
	Info::PeerGifts::Delegate _delegate;
	std::vector<Entry> _entries;

};

GiftPicker::GiftPicker(
	QWidget *parent,
	not_null<Main::Session*> session,
	const std::vector<Data::StarGift> &gifts,
	Fn<void(const Data::StarGift&)> clicked,
	Fn<bool(const Data::StarGift&)> selected)
: RpWidget(parent)
, _selected(std::move(selected))
, _delegate(session, Info::PeerGifts::GiftButtonMode::Minimal) {
	_entries.reserve(gifts.size());
	for (const auto &gift : gifts) {
		auto button = std::make_unique<Info::PeerGifts::GiftButton>(
			this,
			&_delegate);
		button->setDescriptor(
			Info::PeerGifts::GiftTypeStars{
				.info = gift,
				.mine = true,
			},
			Info::PeerGifts::GiftButtonMode::Minimal);
		button->setClickedCallback([=] {
			clicked(gift);
		});
		button->show();
		_entries.push_back({ .gift = gift, .button = std::move(button) });
	}
	refreshSelection();
}

void GiftPicker::refreshSelection() {
	for (const auto &entry : _entries) {
		entry.button->toggleSelected(_selected(entry.gift));
	}
}

int GiftPicker::resizeGetHeight(int newWidth) {
	if (_entries.empty()) {
		return 0;
	}
	const auto &padding = st::sgMoneyGiftsPadding;
	const auto skip = st::sgMoneyGiftsSkip;
	const auto available = newWidth - padding.left() - padding.right();
	const auto single = QSize(
		std::max((available - (kGiftsPerRow - 1) * skip) / kGiftsPerRow, 1),
		_delegate.buttonSize().height());
	const auto extend = _delegate.buttonExtend();
	for (auto i = 0, count = int(_entries.size()); i != count; ++i) {
		const auto row = i / kGiftsPerRow;
		const auto column = i % kGiftsPerRow;
		_entries[i].button->setGeometry(
			QRect(
				QPoint(
					padding.left() + column * (single.width() + skip),
					padding.top() + row * (single.height() + skip)),
				single),
			extend);
	}
	const auto rows = (int(_entries.size()) + kGiftsPerRow - 1)
		/ kGiftsPerRow;
	return padding.top()
		+ rows * single.height()
		+ (rows - 1) * skip
		+ padding.bottom();
}

// Static stickers of the library: favourites, recents, installed packs.
class StickersGrid final : public Ui::RpWidget {
public:
	StickersGrid(
		QWidget *parent,
		not_null<Main::Session*> session,
		std::vector<not_null<DocumentData*>> list,
		Fn<void(not_null<DocumentData*>)> clicked,
		Fn<bool(not_null<DocumentData*>)> selected);

protected:
	int resizeGetHeight(int newWidth) override;
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	[[nodiscard]] int columns() const;
	[[nodiscard]] QRect cellRect(int index) const;
	[[nodiscard]] int indexAt(QPoint point) const;
	void setOver(int index);

	const std::vector<not_null<DocumentData*>> _list;
	const Fn<void(not_null<DocumentData*>)> _clicked;
	const Fn<bool(not_null<DocumentData*>)> _selected;
	base::flat_map<
		not_null<DocumentData*>,
		std::shared_ptr<Data::DocumentMedia>> _media;
	int _over = -1;
	int _pressed = -1;

};

StickersGrid::StickersGrid(
	QWidget *parent,
	not_null<Main::Session*> session,
	std::vector<not_null<DocumentData*>> list,
	Fn<void(not_null<DocumentData*>)> clicked,
	Fn<bool(not_null<DocumentData*>)> selected)
: RpWidget(parent)
, _list(std::move(list))
, _clicked(std::move(clicked))
, _selected(std::move(selected)) {
	setMouseTracking(true);
	session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		update();
	}, lifetime());
}

int StickersGrid::columns() const {
	const auto &padding = st::sgCardEditorStickersPadding;
	const auto size = st::sgCardEditorStickerSize;
	return std::max(
		(width() - padding.left() - padding.right()) / size,
		1);
}

QRect StickersGrid::cellRect(int index) const {
	const auto &padding = st::sgCardEditorStickersPadding;
	const auto size = st::sgCardEditorStickerSize;
	const auto perRow = columns();
	const auto available = width() - padding.left() - padding.right();
	const auto left = padding.left() + (available - perRow * size) / 2;
	return QRect(
		left + (index % perRow) * size,
		padding.top() + (index / perRow) * size,
		size,
		size);
}

int StickersGrid::indexAt(QPoint point) const {
	for (auto i = 0, count = int(_list.size()); i != count; ++i) {
		if (cellRect(i).contains(point)) {
			return i;
		}
	}
	return -1;
}

int StickersGrid::resizeGetHeight(int newWidth) {
	const auto &padding = st::sgCardEditorStickersPadding;
	const auto size = st::sgCardEditorStickerSize;
	const auto perRow = std::max(
		(newWidth - padding.left() - padding.right()) / size,
		1);
	const auto rows = (int(_list.size()) + perRow - 1) / perRow;
	return padding.top() + rows * size + padding.bottom();
}

void StickersGrid::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	const auto clip = e->rect();
	const auto skip = st::sgCardEditorStickerSkip;
	for (auto i = 0, count = int(_list.size()); i != count; ++i) {
		const auto cell = cellRect(i);
		if (!cell.intersects(clip)) {
			continue;
		}
		const auto document = _list[i];
		if (_selected(document) || i == _over) {
			auto hq = PainterHighQualityEnabler(p);
			p.setPen(Qt::NoPen);
			p.setBrush(_selected(document)
				? st::windowBgActive->c
				: st::windowBgOver->c);
			if (_selected(document)) {
				p.setOpacity(0.25);
			}
			p.drawRoundedRect(cell, skip * 2, skip * 2);
			p.setOpacity(1.);
		}
		auto i_media = _media.find(document);
		if (i_media == end(_media)) {
			auto media = document->createMediaView();
			media->checkStickerSmall();
			i_media = _media.emplace(document, std::move(media)).first;
		}
		const auto media = i_media->second.get();
		const auto image = media->getStickerSmall()
			? media->getStickerSmall()
			: media->thumbnail();
		if (!image || image->isNull()) {
			continue;
		}
		const auto inner = cell.marginsRemoved(
			QMargins(skip, skip, skip, skip));
		const auto natural = QSize(
			std::max(image->width(), 1),
			std::max(image->height(), 1));
		const auto size = natural.scaled(inner.size(), Qt::KeepAspectRatio);
		p.drawPixmap(
			inner.x() + (inner.width() - size.width()) / 2,
			inner.y() + (inner.height() - size.height()) / 2,
			image->pix(size));
	}
}

void StickersGrid::setOver(int index) {
	if (_over != index) {
		_over = index;
		setCursor((index >= 0) ? style::cur_pointer : style::cur_default);
		update();
	}
}

void StickersGrid::mouseMoveEvent(QMouseEvent *e) {
	setOver(indexAt(e->pos()));
}

void StickersGrid::mousePressEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
		_pressed = indexAt(e->pos());
	}
}

void StickersGrid::mouseReleaseEvent(QMouseEvent *e) {
	const auto pressed = std::exchange(_pressed, -1);
	if (pressed >= 0 && pressed == indexAt(e->pos())) {
		_clicked(_list[pressed]);
		update();
	}
}

void StickersGrid::leaveEventHook(QEvent *e) {
	setOver(-1);
}

[[nodiscard]] std::vector<not_null<DocumentData*>> CollectStickers(
		not_null<Main::Session*> session) {
	auto result = std::vector<not_null<DocumentData*>>();
	auto seen = base::flat_set<not_null<DocumentData*>>();
	const auto &stickers = session->data().stickers();
	const auto &sets = stickers.sets();
	const auto add = [&](uint64 setId) {
		const auto i = sets.find(setId);
		if (i == sets.end()) {
			return;
		}
		const auto set = i->second.get();
		if (set->flags & Data::StickersSetFlag::Archived) {
			return;
		}
		for (const auto document : set->stickers) {
			if (int(result.size()) >= kStickersLimit) {
				return;
			} else if (IsStaticSticker(document)
				&& seen.emplace(document).second) {
				result.push_back(document);
			}
		}
	};
	add(Data::Stickers::FavedSetId);
	add(Data::Stickers::CloudRecentSetId);
	for (const auto setId : stickers.setsOrder()) {
		add(setId);
	}
	return result;
}

void LoadOwnGifts(
		not_null<Main::Session*> session,
		QString offset,
		std::shared_ptr<std::vector<Data::StarGift>> collected,
		Fn<void()> done,
		not_null<rpl::lifetime*> lifetime) {
	Data::MyUniqueGiftsSlice(
		session,
		Data::MyUniqueType::OnlyOwned,
		offset
	) | rpl::take(1) | rpl::on_next([=](Data::MyGiftsDescriptor slice) {
		auto seen = base::flat_set<QString>();
		for (const auto &gift : *collected) {
			seen.emplace(gift.unique->slug);
		}
		for (auto &saved : slice.list) {
			const auto &unique = saved.info.unique;
			if (unique
				&& !unique->slug.isEmpty()
				&& seen.emplace(unique->slug).second) {
				collected->push_back(std::move(saved.info));
			}
		}
		if (slice.offset.isEmpty()
			|| slice.list.empty()
			|| int(collected->size()) >= kGiftsLimit) {
			done();
		} else {
			LoadOwnGifts(session, slice.offset, collected, done, lifetime);
		}
	}, *lifetime);
}

void CardEditorBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		Card initial) {
	using namespace Mokagram::BoxParts;
	const auto session = &controller->session();
	const auto kind = initial.kind;

	box->setStyle(PillButtonsBox());
	RoundLayer(box);
	box->setWidth(st::boxWideWidth);
	box->setCloseByOutsideClick(false);
	box->setTitle((kind == Kind::Stars)
		? tr::lng_sg_card_edit_stars()
		: tr::lng_sg_card_edit_gram());
	box->addTopButton(st::boxTitleClose, [=] { box->closeBox(); });

	const auto container = box->verticalLayout();
	const auto card = container->add(
		object_ptr<EditorCard>(container, session->user(), initial));

	// The hint, replaced by «Наверх» / «Убрать» while a sticker is selected.
	const auto under = container->add(
		object_ptr<Ui::RpWidget>(container),
		st::sgCardEditorHintPadding);
	const auto hint = Ui::CreateChild<Ui::FlatLabel>(
		under,
		tr::lng_sg_card_editor_hint(
			lt_max,
			rpl::single(QString::number(kMaxStickers))),
		st::sgMoneyEmptyLabel);
	const auto front = Ui::CreateChild<Ui::RoundButton>(
		under,
		tr::lng_sg_card_sticker_to_front(),
		PillLight());
	const auto remove = Ui::CreateChild<Ui::RoundButton>(
		under,
		tr::lng_sg_card_sticker_remove(),
		PillLight());
	front->setTextTransform(Ui::RoundButtonTextTransform::NoTransform);
	remove->setTextTransform(Ui::RoundButtonTextTransform::NoTransform);
	front->setClickedCallback([=] { card->bringSelectedToFront(); });
	remove->setClickedCallback([=] { card->removeSelected(); });
	rpl::combine(
		under->widthValue(),
		hint->heightValue(),
		card->hasSelectionValue()
	) | rpl::on_next([=](int width, int, bool selected) {
		hint->resizeToWidth(width);
		const auto skip = st::sgMoneyGiftsSkip;
		const auto buttons = front->width() + skip + remove->width();
		const auto height = std::max(hint->height(), front->height());
		under->resize(width, height);
		hint->moveToLeft(0, (height - hint->height()) / 2, width);
		const auto left = (width - buttons) / 2;
		front->moveToLeft(left, (height - front->height()) / 2, width);
		remove->moveToLeft(
			left + front->width() + skip,
			(height - remove->height()) / 2,
			width);
		hint->setVisible(!selected);
		front->setVisible(selected);
		remove->setVisible(selected);
	}, under->lifetime());

	const auto tabs = container->add(
		object_ptr<Ui::SettingsSlider>(container, st::defaultTabsSlider),
		st::sgMoneyTabsPadding);
	tabs->setSections({
		tr::lng_sg_card_tab_background(tr::now),
		tr::lng_sg_card_tab_gifts(tr::now),
		tr::lng_sg_card_tab_stickers(tr::now),
	});
	const auto body = container->add(
		object_ptr<Ui::VerticalLayout>(container));

	struct State {
		std::shared_ptr<std::vector<Data::StarGift>> gifts
			= std::make_shared<std::vector<Data::StarGift>>();
		bool giftsLoaded = false;
		int tab = 0;
		Fn<void()> refresh;
		Fn<void()> rebuild;
		bool saving = false;
	};
	const auto state = box->lifetime().make_state<State>();
	const auto tooMany = [=] {
		box->showToast(tr::lng_sg_card_error_too_many(
			tr::now,
			lt_max,
			QString::number(kMaxStickers)));
	};
	const auto addLabel = [=](
			not_null<Ui::VerticalLayout*> to,
			rpl::producer<QString> text) {
		to->add(
			object_ptr<Ui::FlatLabel>(
				to,
				std::move(text),
				st::sgMoneyEmptyLabel),
			st::sgMoneyEmptyPadding);
	};
	const auto fillGifts = [=](
			not_null<Ui::VerticalLayout*> to,
			bool background) {
		if (!state->giftsLoaded) {
			addLabel(to, tr::lng_sg_money_loading());
			return;
		}
		if (background) {
			const auto stock = to->add(object_ptr<Ui::SettingsButton>(
				to,
				tr::lng_sg_card_background_none(),
				st::settingsButtonNoIcon));
			stock->setClickedCallback([=] {
				card->setBackground(nullptr);
			});
		}
		if (state->gifts->empty()) {
			addLabel(to, tr::lng_sg_card_no_gifts());
			return;
		}
		const auto picker = to->add(object_ptr<GiftPicker>(
			to,
			session,
			*state->gifts,
			[=](const Data::StarGift &gift) {
				if (background) {
					const auto now = card->card().background;
					card->setBackground((now
						&& now->slug == gift.unique->slug)
						? nullptr
						: gift.unique);
				} else if (!card->toggleSticker({
						.document = gift.unique->model.document,
						.giftSlug = gift.unique->slug,
					})) {
					tooMany();
				}
			},
			[=](const Data::StarGift &gift) {
				if (background) {
					const auto now = card->card().background;
					return now && (now->slug == gift.unique->slug);
				}
				return card->hasSticker(gift.unique->slug, nullptr);
			}));
		state->refresh = [=] { picker->refreshSelection(); };
	};
	const auto fillStickers = [=](not_null<Ui::VerticalLayout*> to) {
		auto list = CollectStickers(session);
		if (list.empty()) {
			addLabel(to, tr::lng_sg_card_no_stickers());
			return;
		}
		const auto grid = to->add(object_ptr<StickersGrid>(
			to,
			session,
			std::move(list),
			[=](not_null<DocumentData*> document) {
				if (!card->toggleSticker({ .document = document })) {
					tooMany();
				}
			},
			[=](not_null<DocumentData*> document) {
				return card->hasSticker(QString(), document);
			}));
		state->refresh = [=] { grid->update(); };
	};
	state->rebuild = [=] {
		state->refresh = nullptr;
		body->clear();
		const auto inner = body->add(object_ptr<Ui::VerticalLayout>(body));
		if (state->tab == 2) {
			fillStickers(inner);
		} else {
			fillGifts(inner, state->tab == 0);
		}
		body->resizeToWidth(container->width());
	};
	tabs->sectionActivated() | rpl::on_next([=](int tab) {
		state->tab = tab;
		state->rebuild();
	}, tabs->lifetime());
	card->changes() | rpl::on_next([=] {
		if (const auto refresh = state->refresh) {
			refresh();
		}
	}, card->lifetime());
	state->rebuild();

	LoadOwnGifts(session, QString(), state->gifts, crl::guard(box, [=] {
		state->giftsLoaded = true;
		if (state->tab != 2) {
			state->rebuild();
		}
	}), &box->lifetime());
	session->api().updateStickers();
	session->data().stickers().updated(
		Data::StickersType::Stickers
	) | rpl::filter([=] {
		return state->tab == 2;
	}) | rpl::on_next([=] {
		state->rebuild();
	}, box->lifetime());

	const auto weak = base::make_weak(controller);
	const auto boxWeak = base::make_weak(box);
	const auto save = [=] {
		if (state->saving) {
			return;
		}
		state->saving = true;
		const auto &now = card->card();
		SaveCard(
			session,
			kind,
			now.background ? now.background->slug : QString(),
			now.stickers,
			[=] {
				if (const auto strong = weak.get()) {
					strong->showToast(tr::lng_sg_card_saved(tr::now));
				}
				if (const auto strong = boxWeak.get()) {
					strong->closeBox();
				}
			},
			[=](const QString &type) {
				state->saving = false;
				const auto strong = weak.get();
				if (!strong) {
					return;
				} else if (type == u"WALLET_CARD_NOT_PURCHASED"_q) {
					if (const auto box = boxWeak.get()) {
						box->closeBox();
					}
					ShowCardsPurchaseBox(strong);
				} else if (type != u"FROZEN_METHOD_INVALID"_q
					|| !strong->uiShow()->showFrozenError()) {
					if (const auto box = boxWeak.get()) {
						box->showToast(ErrorText(type));
					}
				}
			});
	};
	box->addButton(tr::lng_sg_card_save(), save, PillActive());
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); }, PillLight());
}

class PromoCard final : public Ui::RpWidget {
public:
	PromoCard(QWidget *parent, not_null<UserData*> user)
	: RpWidget(parent)
	, _face(user, Kind::Gram, [=] { update(); }) {
	}

protected:
	int resizeGetHeight(int newWidth) override {
		const auto &padding = st::sgCardEditorCardPadding;
		return padding.top()
			+ CardHeight(newWidth - padding.left() - padding.right())
			+ padding.bottom();
	}
	void paintEvent(QPaintEvent *e) override {
		auto p = QPainter(this);
		const auto &padding = st::sgCardEditorCardPadding;
		const auto width = this->width() - padding.left() - padding.right();
		_face.paint(
			p,
			QRect(padding.left(), padding.top(), width, CardHeight(width)),
			true);
	}

private:
	CardFace _face;

};

void PurchaseBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	using namespace Mokagram::BoxParts;
	box->setStyle(PillButtonsBox());
	RoundLayer(box);
	box->setWidth(st::boxWideWidth);
	// StaticGram: the title in the box header, so the close cross sits in
	// its usual place next to it and not over the card corner.
	box->setTitle(tr::lng_sg_cards_promo_title());
	box->addTopButton(st::boxTitleClose, [=] { box->closeBox(); });

	const auto container = box->verticalLayout();
	container->add(object_ptr<PromoCard>(
		container,
		controller->session().user()));
	container->add(
		object_ptr<Ui::FlatLabel>(
			container,
			tr::lng_sg_cards_promo_text(),
			st::sgMoneyAboutLabel),
		st::sgMoneyAboutPadding,
		style::al_top);
	const auto weak = base::make_weak(controller);
	// A full-width main button under the text instead of a small pill.
	StaticGram::BoxParts::AddMainButton(
		container,
		tr::lng_sg_cards_promo_button(),
		[=] {
			box->closeBox();
			if (const auto strong = weak.get()) {
				OpenCardsPurchase(strong);
			}
		},
		6);
	Ui::AddSkip(container);
}

} // namespace

void ShowCardsPurchaseBox(not_null<Window::SessionController*> controller) {
	controller->show(Box(PurchaseBox, controller));
}

void ShowCardEditor(
		not_null<Window::SessionController*> controller,
		Kind kind) {
	const auto self = controller->session().user();
	const auto weak = base::make_weak(controller);
	RefreshCards(self);
	CardsValue(self) | rpl::filter([](const Cards &cards) {
		return cards.loaded || cards.failed;
	}) | rpl::take(1) | rpl::on_next([=](const Cards &cards) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		} else if (!cards.loaded) {
			strong->showToast(tr::lng_sg_money_load_failed(tr::now));
			return;
		} else if (!cards.purchased) {
			// Android SGMoneyActivity.openEditor: SGCardsActivity promo.
			ShowCardsPurchaseBox(strong);
			return;
		}
		auto card = Card{ .kind = kind };
		if (const auto existing = cards.find(kind)) {
			card = *existing;
		}
		strong->show(Box(CardEditorBox, strong, std::move(card)));
	}, controller->lifetime());
}

} // namespace StaticGram::Money
