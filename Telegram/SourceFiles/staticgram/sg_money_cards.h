/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "staticgram/sg_money_api.h"
#include "ui/effects/animations.h"
#include "ui/rp_widget.h"

namespace Ui::Text {
class CustomEmoji;
} // namespace Ui::Text

namespace StaticGram::Money {

// One card face (Android SGCardView / WalletCardComponent.setSgCard): the
// unique gift background (backdrop gradient + pattern) or the stock look,
// the stickers, the balance with its rouble value, the card number and the
// holder name.
class CardFace final {
public:
	CardFace(not_null<UserData*> user, Kind kind, Fn<void()> repaint);
	~CardFace();

	void setCard(const Card *card);
	void setBalance(int64 balance);

	[[nodiscard]] Kind kind() const;
	[[nodiscard]] int64 balance() const;

	void paint(QPainter &p, QRect rect, bool active);

private:
	struct Sticker;

	void paintBackground(QPainter &p, QRect rect);
	void paintStickers(QPainter &p, QRect rect, bool active);
	void paintTexts(QPainter &p, QRect rect);
	void loadSticker(not_null<Sticker*> sticker);

	const not_null<UserData*> _user;
	const Kind _kind;
	const Fn<void()> _repaint;

	std::shared_ptr<Data::UniqueGift> _background;
	std::unique_ptr<Ui::Text::CustomEmoji> _pattern;
	base::flat_map<float64, QImage> _patternCache;
	QImage _gradient;
	std::vector<std::unique_ptr<Sticker>> _stickers;
	int64 _balance = 0;
	int _lastWidth = 0;

};

// The swipeable GRAM / Stars pair (Android SGMoneyCardsPager).
class CardsPager final : public Ui::RpWidget {
public:
	CardsPager(QWidget *parent, not_null<UserData*> user, Kind initial);
	~CardsPager();

	void setCards(const Cards &cards);
	void setBalance(Kind kind, int64 balance);
	void showKind(Kind kind);

	[[nodiscard]] Kind kind() const;
	[[nodiscard]] rpl::producer<Kind> kindValue() const;
	[[nodiscard]] int64 balance(Kind kind) const;

protected:
	int resizeGetHeight(int newWidth) override;
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void wheelEvent(QWheelEvent *e) override;

private:
	[[nodiscard]] int cardWidth() const;
	[[nodiscard]] int cardHeight() const;
	[[nodiscard]] QRect cardRect(int index, float64 position) const;
	[[nodiscard]] QRect dotsRect() const;
	[[nodiscard]] int indexAt(QPoint point) const;
	void animateTo(int index);

	std::array<std::unique_ptr<CardFace>, 2> _faces;
	rpl::variable<int> _index = 0;
	float64 _position = 0.;
	Ui::Animations::Simple _animation;
	std::optional<QPoint> _pressed;
	float64 _pressedPosition = 0.;
	bool _dragging = false;

};

[[nodiscard]] int IndexByKind(Kind kind);
[[nodiscard]] Kind KindByIndex(int index);

} // namespace StaticGram::Money
