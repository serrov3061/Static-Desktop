/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_settings_boxes.h"

#include "staticgram/sg_avatar_frames.h"
#include "staticgram/sg_badges.h"
#include "staticgram/sg_badges_bot.h"
#include "staticgram/sg_banner.h"
#include "staticgram/sg_box_parts.h"
#include "staticgram/sg_frame_params.h"
#include "boxes/peers/edit_peer_color_box.h" // ButtonStyleWithRightEmoji.
#include "core/file_utilities.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/stickers/data_custom_emoji.h"
#include "info/profile/info_profile_emoji_status_panel.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mokagram/ui/mokagram_box_parts.h"
#include "settings/settings_common.h"
#include "ui/basic_click_handlers.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/text/text_custom_emoji.h"
#include "ui/userpic_view.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_basic.h"
#include "styles/style_dialogs.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_sg_ui.h"
#include "styles/style_staticgram.h"
#include "styles/style_widgets.h"

#include <QtGui/QPainterPath>

namespace StaticGram {
namespace {

constexpr auto kMaxBadgeTextLength = 70;

// Rows and chips inside a mokaGram card: the card already has the side gap.
const auto kCardRowMargins = QMargins(16, 2, 16, 10);
const auto kCardSliderMargins = QMargins(16, 8, 16, 12);

using namespace StaticGram::BoxParts;

[[nodiscard]] uint64 SelfId(not_null<Main::Session*> session) {
	return uint64(session->userId().bare);
}

[[nodiscard]] int EmojiSize() {
	return Data::FrameSizeFromTag({}) / style::DevicePixelRatio();
}

[[nodiscard]] QString KeyName(const QString &key) {
	if (key == u"palette"_q) {
		return tr::lng_sg_frame_palette(tr::now);
	} else if (key == u"metal"_q) {
		return tr::lng_sg_frame_metal(tr::now);
	} else if (key == u"gem"_q) {
		return tr::lng_sg_frame_gem(tr::now);
	} else if (key == u"gempos"_q) {
		return tr::lng_sg_frame_gempos(tr::now);
	} else if (key == u"embers"_q) {
		return tr::lng_sg_frame_embers(tr::now);
	} else if (key == u"stars"_q) {
		return tr::lng_sg_frame_stars(tr::now);
	} else if (key == u"planet"_q) {
		return tr::lng_sg_frame_planet(tr::now);
	} else if (key == u"meteors"_q) {
		return tr::lng_sg_frame_meteors(tr::now);
	} else if (key == u"snow"_q) {
		return tr::lng_sg_frame_snow(tr::now);
	} else if (key == u"cap"_q) {
		return tr::lng_sg_frame_cap(tr::now);
	} else if (key == u"whiskers"_q) {
		return tr::lng_sg_frame_whiskers(tr::now);
	} else if (key == u"paw"_q) {
		return tr::lng_sg_frame_paw(tr::now);
	} else if (key == u"wings"_q) {
		return tr::lng_sg_frame_wings(tr::now);
	} else if (key == u"sparkle"_q) {
		return tr::lng_sg_frame_sparkle(tr::now);
	} else if (key == u"tail"_q) {
		return tr::lng_sg_frame_tail(tr::now);
	} else if (key == u"pulse"_q) {
		return tr::lng_sg_frame_pulse(tr::now);
	}
	return key;
}

[[nodiscard]] QString OptionName(const QString &value) {
	static const auto map = base::flat_map<QString, tr::phrase<>>{
		{ u"fire"_q, tr::lng_sg_frame_opt_fire },
		{ u"blue"_q, tr::lng_sg_frame_opt_blue },
		{ u"green"_q, tr::lng_sg_frame_opt_green },
		{ u"purple"_q, tr::lng_sg_frame_opt_purple },
		{ u"pink"_q, tr::lng_sg_frame_opt_pink },
		{ u"rainbow"_q, tr::lng_sg_frame_opt_rainbow },
		{ u"pastel"_q, tr::lng_sg_frame_opt_pastel },
		{ u"neon"_q, tr::lng_sg_frame_opt_neon },
		{ u"mono"_q, tr::lng_sg_frame_opt_mono },
		{ u"gold"_q, tr::lng_sg_frame_opt_gold },
		{ u"platinum"_q, tr::lng_sg_frame_opt_platinum },
		{ u"rose"_q, tr::lng_sg_frame_opt_rose },
		{ u"black"_q, tr::lng_sg_frame_opt_black },
		{ u"diamond"_q, tr::lng_sg_frame_opt_diamond },
		{ u"ruby"_q, tr::lng_sg_frame_opt_ruby },
		{ u"emerald"_q, tr::lng_sg_frame_opt_emerald },
		{ u"sapphire"_q, tr::lng_sg_frame_opt_sapphire },
		{ u"amethyst"_q, tr::lng_sg_frame_opt_amethyst },
		{ u"none"_q, tr::lng_sg_frame_opt_none },
		{ u"bottom"_q, tr::lng_sg_frame_opt_bottom },
		{ u"top"_q, tr::lng_sg_frame_opt_top },
		{ u"right"_q, tr::lng_sg_frame_opt_right },
		{ u"violet"_q, tr::lng_sg_frame_opt_violet },
		{ u"red"_q, tr::lng_sg_frame_opt_red },
		{ u"teal"_q, tr::lng_sg_frame_opt_teal },
		{ u"ice"_q, tr::lng_sg_frame_opt_ice },
		{ u"snow"_q, tr::lng_sg_frame_opt_snow },
		{ u"aurora"_q, tr::lng_sg_frame_opt_aurora },
		{ u"white"_q, tr::lng_sg_frame_opt_white },
		{ u"ginger"_q, tr::lng_sg_frame_opt_ginger },
		{ u"lilac"_q, tr::lng_sg_frame_opt_lilac },
		{ u"silver"_q, tr::lng_sg_frame_opt_silver },
		{ u"sky"_q, tr::lng_sg_frame_opt_sky },
	};
	const auto i = map.find(value);
	return (i != end(map)) ? i->second(tr::now) : value;
}

// A row of choice chips (palette / metal / gem / gem position / speed),
// wrapped into several lines when needed.
class ChipsRow final : public Ui::RpWidget {
public:
	struct Option {
		QString value;
		QString label;
		std::optional<QColor> dot;
	};

	ChipsRow(
		QWidget *parent,
		std::vector<Option> options,
		rpl::producer<QString> selected,
		Fn<void(QString)> chosen);

protected:
	int resizeGetHeight(int newWidth) override;
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	[[nodiscard]] int chipWidth(const Option &option) const;
	[[nodiscard]] int indexAt(QPoint point) const;
	void setOver(int index);

	std::vector<Option> _options;
	std::vector<QRect> _rects;
	QString _selected;
	Fn<void(QString)> _chosen;
	int _over = -1;
	int _pressed = -1;

};

ChipsRow::ChipsRow(
		QWidget *parent,
		std::vector<Option> options,
		rpl::producer<QString> selected,
		Fn<void(QString)> chosen)
: RpWidget(parent)
, _options(std::move(options))
, _chosen(std::move(chosen)) {
	setMouseTracking(true);
	std::move(selected) | rpl::on_next([=](QString value) {
		_selected = value;
		update();
	}, lifetime());
}

int ChipsRow::chipWidth(const Option &option) const {
	const auto &font = st::sgFrameChipFontSelected;
	return st::sgFrameChipPadding * 2
		+ (option.dot ? (st::sgFrameChipDot + st::sgFrameChipDotSkip) : 0)
		+ font->width(option.label);
}

int ChipsRow::resizeGetHeight(int newWidth) {
	_rects.clear();
	auto x = 0;
	auto y = 0;
	const auto height = st::sgFrameChipHeight;
	const auto skip = st::sgFrameChipSkip;
	for (const auto &option : _options) {
		const auto width = std::min(chipWidth(option), newWidth);
		if (x > 0 && x + width > newWidth) {
			x = 0;
			y += height + skip;
		}
		_rects.push_back(QRect(x, y, width, height));
		x += width + skip;
	}
	return _rects.empty() ? 0 : (y + height);
}

void ChipsRow::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);
	const auto line = st::sgFrameChipLine;
	for (auto i = 0; i != int(_options.size()) && i != int(_rects.size()); ++i) {
		const auto &option = _options[i];
		const auto rect = QRectF(_rects[i]).marginsRemoved(
			QMarginsF(line / 2., line / 2., line / 2., line / 2.));
		const auto selected = (option.value == _selected);
		const auto radius = rect.height() / 2.;
		auto accent = st::activeButtonBg->c;
		if (selected) {
			auto fill = accent;
			fill.setAlphaF(0.14);
			p.setPen(QPen(accent, line));
			p.setBrush(fill);
		} else {
			p.setPen(Qt::NoPen);
			p.setBrush((i == _over) ? st::windowBgRipple : st::windowBgOver);
		}
		p.drawRoundedRect(rect, radius, radius);
		auto x = _rects[i].x() + st::sgFrameChipPadding;
		if (option.dot) {
			const auto size = st::sgFrameChipDot;
			const auto dot = QRectF(
				x,
				_rects[i].y() + (_rects[i].height() - size) / 2.,
				size,
				size);
			p.setPen(QPen(QColor(0, 0, 0, 0x33), 1.));
			p.setBrush(*option.dot);
			p.drawEllipse(dot);
			x += size + st::sgFrameChipDotSkip;
		}
		const auto &font = selected
			? st::sgFrameChipFontSelected
			: st::sgFrameChipFont;
		p.setFont(font);
		p.setPen(selected ? st::windowActiveTextFg : st::windowFg);
		const auto available = _rects[i].x()
			+ _rects[i].width()
			- st::sgFrameChipPadding
			- x;
		p.drawText(
			x,
			_rects[i].y() + (_rects[i].height() - font->height) / 2
				+ font->ascent,
			font->elided(option.label, std::max(available, 0)));
	}
}

int ChipsRow::indexAt(QPoint point) const {
	for (auto i = 0; i != int(_rects.size()); ++i) {
		if (_rects[i].contains(point)) {
			return i;
		}
	}
	return -1;
}

void ChipsRow::setOver(int index) {
	if (_over != index) {
		_over = index;
		setCursor((_over >= 0) ? style::cur_pointer : style::cur_default);
		update();
	}
}

void ChipsRow::mouseMoveEvent(QMouseEvent *e) {
	setOver(indexAt(e->pos()));
}

void ChipsRow::mousePressEvent(QMouseEvent *e) {
	_pressed = indexAt(e->pos());
}

void ChipsRow::mouseReleaseEvent(QMouseEvent *e) {
	const auto pressed = std::exchange(_pressed, -1);
	if (pressed >= 0
		&& pressed == indexAt(e->pos())
		&& pressed < int(_options.size())) {
		const auto value = _options[pressed].value;
		if (_chosen) {
			_chosen(value);
		}
	}
}

void ChipsRow::leaveEventHook(QEvent *e) {
	setOver(-1);
}

void PaintCustomEmoji(
		QPainter &p,
		Ui::Text::CustomEmoji *emoji,
		QPoint position,
		double opacity = 1.) {
	if (!emoji) {
		return;
	}
	const auto was = p.opacity();
	p.setOpacity(was * opacity);
	emoji->paint(p, {
		.textColor = st::dialogsVerifiedIconBg->c,
		.now = crl::now(),
		.position = position,
		.paused = !AvatarFramesAnimated(),
	});
	p.setOpacity(was);
}

// A check / lock mark at the bottom-right corner of a picker avatar.
void PaintMark(QPainter &p, QPointF center, bool check) {
	const auto outer = st::sgFramePickerCheckOuter;
	const auto inner = st::sgFramePickerCheckInner;
	p.setPen(Qt::NoPen);
	p.setBrush(st::windowBg);
	p.drawEllipse(center, outer, outer);
	p.setBrush(check ? st::activeButtonBg : st::windowSubTextFg);
	p.drawEllipse(center, inner, inner);
	const auto u = inner / 7.;
	auto pen = QPen(
		check ? st::activeButtonFg->c : st::windowBg->c,
		st::sgFramePickerCheckLine);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	if (check) {
		auto path = QPainterPath(center + QPointF(-3.5 * u, 0.));
		path.lineTo(center + QPointF(-1. * u, 2.5 * u));
		path.lineTo(center + QPointF(3.5 * u, -2.5 * u));
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.drawPath(path);
	} else {
		p.setBrush(st::windowBg);
		p.drawRoundedRect(
			QRectF(center.x() - 3. * u, center.y() - 0.5 * u, 6. * u, 4. * u),
			u,
			u);
		pen.setWidthF(std::max(1., u * 1.2));
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.drawArc(
			QRectF(center.x() - 2. * u, center.y() - 4. * u, 4. * u, 5. * u),
			0,
			180 * 16);
	}
}

void OpenShop(not_null<Main::Session*> session) {
	const auto url = ReadBadgesConfig(session).shopUrl;
	if (!url.isEmpty()) {
		// t.me links are opened outside, in Telegram (like Android).
		UrlClickHandler::Open(url);
	}
}

[[nodiscard]] int BadgeState(not_null<Main::Session*> session) {
	const auto badge = BadgeOfUser(SelfId(session));
	return (badge.developer ? 4 : 0)
		| (badge.canChange ? 2 : 0)
		| (badge ? 1 : 0);
}

void AddFeature(
		not_null<Ui::VerticalLayout*> container,
		const style::icon &icon,
		rpl::producer<QString> title,
		rpl::producer<QString> text) {
	const auto wrap = container->add(object_ptr<Ui::RpWidget>(container));
	const auto padding = st::sgSupporterFeaturePadding;
	const auto inner = Ui::CreateChild<Ui::VerticalLayout>(wrap);
	inner->add(object_ptr<Ui::FlatLabel>(
		inner,
		std::move(title),
		st::sgSupporterFeatureTitle));
	inner->add(
		object_ptr<Ui::FlatLabel>(
			inner,
			std::move(text),
			st::sgSupporterFeatureText),
		QMargins(0, st::sgSupporterFeatureTextSkip, 0, 0));
	const auto iconWidget = Ui::CreateChild<Ui::RpWidget>(wrap);
	iconWidget->resize(icon.size());
	iconWidget->paintRequest() | rpl::on_next([=, &icon] {
		auto p = QPainter(iconWidget);
		icon.paint(p, 0, 0, iconWidget->width(), st::windowBoldFg->c);
	}, iconWidget->lifetime());
	wrap->widthValue() | rpl::on_next([=](int width) {
		inner->resizeToWidth(
			std::max(width - padding.left() - padding.right(), 1));
		inner->moveToLeft(padding.left(), padding.top());
		iconWidget->moveToLeft(
			st::sgSupporterFeatureIconLeft,
			padding.top() + st::sgSupporterFeatureIconTop);
	}, wrap->lifetime());
	inner->heightValue() | rpl::on_next([=](int height) {
		wrap->resize(wrap->width(), padding.top() + height + padding.bottom());
	}, wrap->lifetime());
}

void SupporterBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		bool fromBadgeBox) {
	const auto session = &controller->session();
	const auto config = ReadBadgesConfig(session);
	SetupBox(box);
	box->setWidth(st::boxWideWidth);
	box->setNoContentMargin(true);

	struct State {
		std::unique_ptr<Ui::Text::CustomEmoji> emoji;
	};
	const auto state = box->lifetime().make_state<State>();
	const auto icon = box->addRow(
		object_ptr<Ui::RpWidget>(box),
		st::sgSupporterIconPadding,
		style::al_top);
	icon->resize(st::sgSupporterIcon, st::sgSupporterIcon);
	if (config.defaultDocumentId) {
		state->emoji = session->data().customEmojiManager().create(
			config.defaultDocumentId,
			[=] { icon->update(); },
			Data::CustomEmojiSizeTag::Large);
	}
	icon->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(icon);
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		p.setBrush(st::activeButtonBg);
		p.drawEllipse(icon->rect());
		if (state->emoji) {
			const auto size = Data::FrameSizeFromTag(
				Data::CustomEmojiSizeTag::Large) / style::DevicePixelRatio();
			state->emoji->paint(p, {
				.textColor = st::activeButtonFg->c,
				.now = crl::now(),
				.position = QPoint(
					(icon->width() - size) / 2,
					(icon->height() - size) / 2),
				.paused = !AvatarFramesAnimated(),
			});
		} else {
			st::menuIconStar.paintInCenter(
				p,
				icon->rect(),
				st::activeButtonFg->c);
		}
	}, icon->lifetime());

	box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_sg_badge_sheet_title(),
			st::sgSupporterTitle),
		st::sgSupporterTitlePadding,
		style::al_top);
	box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_sg_badge_sheet_info(),
			st::sgSupporterText),
		st::sgSupporterTextPadding,
		style::al_top);
	const auto container = box->verticalLayout();
	const auto features = AddCard(container, 4, 6);
	AddFeature(
		features,
		st::menuIconPayment,
		tr::lng_sg_badge_step1_title(),
		tr::lng_sg_badge_step1_info(
			lt_supporter,
			rpl::single(config.priceSupporter),
			lt_custom,
			rpl::single(config.priceCustom),
			lt_upgrade,
			rpl::single(config.priceUpgrade)));
	AddFeature(
		features,
		st::menuIconShop,
		tr::lng_sg_badge_step2_title(),
		tr::lng_sg_badge_step2_info());
	AddFeature(
		features,
		st::menuIconStar,
		tr::lng_sg_badge_step3_title(),
		tr::lng_sg_badge_step3_info());
	Ui::AddSkip(features);
	Ui::AddSkip(container);

	// The primary action depends on what the user already has.
	const auto badge = BadgeOfUser(SelfId(session));
	if (!badge.developer) {
		if (badge.canChange) {
			box->addButton(tr::lng_sg_badge_settings(), [=] {
				box->closeBox();
				if (!fromBadgeBox) {
					ShowBadgeBox(controller);
				}
			}, Mokagram::BoxParts::PillActive());
		} else if (!config.shopUrl.isEmpty()) {
			box->addButton(
				(badge
					? tr::lng_sg_badge_upgrade()
					: tr::lng_sg_badge_buy()),
				[=] {
					box->closeBox();
					OpenShop(session);
				},
				Mokagram::BoxParts::PillActive());
		}
	}
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });

	// The server state may differ from what the box was built for.
	const auto stateAtOpen = BadgeState(session);
	SyncBadgesNow(crl::guard(box, [=] {
		if (BadgeState(session) != stateAtOpen) {
			box->closeBox();
			ShowSupporterBox(controller, fromBadgeBox);
		}
	}));
}

void BadgeBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	const auto user = session->user();
	const auto selfId = SelfId(session);
	SetupBox(box);
	box->setTitle(tr::lng_sg_badge_title());
	box->setWidth(st::boxWideWidth);
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });

	struct State {
		DocumentId pending = 0;
		bool loaded = false;
		bool saving = false;
		int kind = -1; // 0 - none, 1 - standard, 2 - custom.
		Info::Profile::EmojiStatusPanel panel;
		Ui::InputField *field = nullptr;
		Ui::PeerUserpicView userpic;
		std::unique_ptr<Ui::Text::CustomEmoji> statusEmoji;
		std::unique_ptr<Ui::Text::CustomEmoji> badgeEmoji;
		std::unique_ptr<Ui::Text::CustomEmoji> buttonEmoji;
		EmojiStatusId statusId;
		DocumentId badgeId = 0;
		DocumentId buttonId = 0;
		rpl::event_stream<> pendingChanges;
	};
	const auto state = box->lifetime().make_state<State>();
	const auto current = [=] {
		return BadgeOfUser(selfId);
	};
	const auto kind = [=] {
		const auto badge = current();
		return !badge ? 0 : badge.canChange ? 2 : 1;
	};
	const auto pendingText = [=] {
		return state->field
			? state->field->getLastText().trimmed()
			: current().text;
	};
	const auto pendingId = [=] {
		return (kind() == 2 && state->pending)
			? state->pending
			: current().documentId;
	};
	const auto edited = [=] {
		const auto badge = current();
		return badge
			&& (pendingId() != badge.documentId
				|| pendingText() != badge.text.trimmed());
	};

	const auto container = box->verticalLayout();
	const auto previewCard = AddCard(container, 0, 6);
	AddCardTitle(previewCard, tr::lng_sg_badge_preview());
	const auto preview = previewCard->add(
		object_ptr<Ui::RpWidget>(previewCard),
		QMargins());
	const auto previewPadding = st::sgBadgePreviewPadding;
	preview->resize(
		st::boxWideWidth,
		previewPadding.top()
			+ st::sgBadgePreviewUserpic
			+ previewPadding.bottom());
	session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		preview->update();
	}, preview->lifetime());
	preview->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(preview);
		auto hq = PainterHighQualityEnabler(p);
		const auto size = st::sgBadgePreviewUserpic;
		const auto left = previewPadding.left();
		const auto top = (preview->height() - size) / 2;
		user->paintUserpic(p, state->userpic, left, top, size);
		if (const auto frame = UserAvatarFrame(selfId)) {
			PaintAvatarFrame(
				p,
				*frame,
				QRectF(left, top, size, size),
				QRectF(preview->rect()),
				1.,
				AvatarFramesAnimated(),
				AvatarFramesLight());
		}
		const auto x = left + size + st::sgBadgePreviewSkip;
		const auto available = preview->width()
			- x
			- previewPadding.right();
		const auto &nameFont = st::sgBadgePreviewNameFont;
		const auto &textFont = st::normalFont;
		const auto emoji = EmojiSize();
		const auto lineHeight = std::max(nameFont->height, emoji);
		const auto total = lineHeight
			+ st::sgBadgePreviewLineSkip
			+ textFont->height;
		const auto y = (preview->height() - total) / 2;

		const auto statusId = user->emojiStatusId();
		if (state->statusId != statusId) {
			state->statusId = statusId;
			state->statusEmoji = statusId
				? session->data().customEmojiManager().create(
					Data::EmojiStatusCustomId(statusId),
					[=] { preview->update(); })
				: nullptr;
		}
		const auto badge = current();
		const auto placeholder = !badge;
		const auto badgeId = placeholder
			? ReadBadgesConfig(session).defaultDocumentId
			: pendingId();
		if (state->badgeId != badgeId) {
			state->badgeId = badgeId;
			state->badgeEmoji = badgeId
				? session->data().customEmojiManager().create(
					badgeId,
					[=] { preview->update(); })
				: nullptr;
		}
		const auto emojiSkip = st::sgBadgePreviewEmojiSkip;
		const auto emojis = (state->statusEmoji ? (emoji + emojiSkip) : 0)
			+ (state->badgeEmoji ? (emoji + emojiSkip) : 0);
		const auto name = nameFont->elided(
			user->name(),
			std::max(available - emojis, 0));
		p.setFont(nameFont);
		p.setPen(st::windowBoldFg);
		p.drawText(
			x,
			y + (lineHeight - nameFont->height) / 2 + nameFont->ascent,
			name);
		auto emojiLeft = x + nameFont->width(name) + emojiSkip;
		const auto emojiTop = y + (lineHeight - emoji) / 2;
		if (state->statusEmoji) {
			PaintCustomEmoji(
				p,
				state->statusEmoji.get(),
				QPoint(emojiLeft, emojiTop));
			emojiLeft += emoji + emojiSkip;
		}
		PaintCustomEmoji(
			p,
			state->badgeEmoji.get(),
			QPoint(emojiLeft, emojiTop),
			placeholder ? st::sgBadgePreviewPlaceholderOpacity : 1.);

		const auto text = placeholder
			? tr::lng_status_online(tr::now)
			: (kind() == 2)
			? pendingText()
			: badge.text.trimmed();
		const auto caption = !text.isEmpty()
			? text
			: badge.developer
			? tr::lng_sg_badge_developer(tr::now, lt_name, user->firstName)
			: tr::lng_sg_badge_supporter(
				tr::now,
				lt_name,
				user->firstName);
		p.setFont(textFont);
		p.setPen(st::windowSubTextFg);
		p.drawText(
			x,
			y + lineHeight + st::sgBadgePreviewLineSkip + textFont->ascent,
			textFont->elided(caption, available));
	}, preview->lifetime());

	const auto body = container->add(
		object_ptr<Ui::VerticalLayout>(container));

	const auto save = [=] {
		if (state->saving) {
			return;
		}
		const auto id = pendingId();
		if (!id) {
			return;
		} else if (ReadBadgesConfig(session).bot.isEmpty()) {
			box->showToast(tr::lng_sg_badge_unavailable(tr::now));
			return;
		}
		state->saving = true;
		ApplyBadge(session, id, pendingText(), crl::guard(box, [=](
				QString result) {
			state->saving = false;
			if (result == u"ok"_q) {
				box->showToast(tr::lng_sg_badge_saved(tr::now));
			} else {
				box->showToast(tr::lng_sg_badge_save_error(
					tr::now,
					lt_reason,
					(result.isEmpty()
						? tr::lng_sg_badge_unknown_error(tr::now)
						: result)));
			}
			preview->update();
		}));
	};

	const auto fill = [=] {
		state->field = nullptr;
		state->buttonEmoji = nullptr;
		state->buttonId = 0;
		body->clear();
		state->kind = kind();
		const auto config = ReadBadgesConfig(session);

		const auto frameCard = AddCard(body);
		Settings::AddButtonWithLabel(
			frameCard,
			tr::lng_sg_frame_title(),
			SelfAvatarFrameNameValue(session),
			st::settingsButton,
			{ &st::menuIconUniqueProfile }
		)->addClickHandler([=] {
			ShowFrameBox(controller);
		});

		if (state->kind == 2) {
			if (!state->loaded) {
				state->loaded = true;
				state->pending = current().documentId;
			}
			const auto badgeCard = AddCard(body);
			const auto button = ButtonStyleWithRightEmoji(
				badgeCard,
				tr::lng_sg_frame_none(tr::now),
				st::settingsButton);
			const auto emojiButton = badgeCard->add(Settings::CreateButtonWithIcon(
				badgeCard,
				tr::lng_sg_badge_emoji(),
				*button.st,
				{ &st::menuIconEmoji }));
			const auto right = Ui::CreateChild<Ui::RpWidget>(emojiButton);
			right->show();
			right->resize(button.emojiWidth + button.added, right->height());
			rpl::combine(
				emojiButton->sizeValue(),
				right->widthValue()
			) | rpl::on_next([=](QSize outer, int width) {
				right->resize(width, outer.height());
				const auto skip = st::settingsButton.padding.right();
				right->moveToRight(skip - button.added, 0, outer.width());
			}, right->lifetime());
			right->paintRequest() | rpl::on_next([=] {
				if (state->panel.paintBadgeFrame(right)) {
					return;
				}
				const auto id = pendingId();
				if (state->buttonId != id) {
					state->buttonId = id;
					state->buttonEmoji = id
						? session->data().customEmojiManager().create(
							id,
							[=] { right->update(); })
						: nullptr;
				}
				auto p = QPainter(right);
				PaintCustomEmoji(
					p,
					state->buttonEmoji.get(),
					QPoint(
						button.added,
						(right->height() - button.emojiWidth) / 2));
			}, right->lifetime());
			emojiButton->setClickedCallback([=] {
				state->panel.show({
					.controller = controller,
					.button = right,
					.ensureAddedEmojiId = { .documentId = pendingId() },
					.channelStatusMode = true,
				});
			});

			const auto textCard = AddCard(body);
			AddCardTitle(textCard, tr::lng_sg_badge_text());
			const auto field = textCard->add(
				object_ptr<Ui::InputField>(
					textCard,
					Mokagram::BoxParts::CardField(st::defaultInputField),
					tr::lng_sg_badge_text_hint(),
					current().text),
				QMargins());
			field->setMaxLength(kMaxBadgeTextLength);
			field->changes() | rpl::on_next([=] {
				preview->update();
			}, field->lifetime());
			field->submits() | rpl::on_next([=] {
				save();
			}, field->lifetime());
			state->field = field;
			AddCardNote(body, tr::lng_sg_badge_custom_info());
			AddMainButton(body, tr::lng_sg_badge_save(), save, 4);
		} else if (state->kind == 1) {
			AddCardNote(body, tr::lng_sg_badge_standard_info());
			if (!config.shopUrl.isEmpty()) {
				AddMainButton(body, tr::lng_sg_badge_upgrade(), [=] {
					OpenShop(session);
				}, 4);
			}
		} else {
			AddCardNote(
				body,
				(config.apiUrl.isEmpty() && config.bot.isEmpty())
					? tr::lng_sg_badge_unavailable()
					: tr::lng_sg_badge_none_info());
			if (!config.shopUrl.isEmpty()) {
				AddMainButton(body, tr::lng_sg_badge_buy(), [=] {
					ShowSupporterBox(controller, true);
				}, 4);
			}
		}
		body->resizeToWidth(container->width());
	};
	fill();

	state->panel.someCustomChosen(
	) | rpl::on_next([=](Info::Profile::EmojiStatusPanel::CustomChosen chosen) {
		const auto config = ReadBadgesConfig(session);
		const auto fallback = current().developer
			? config.developerDocumentId
			: config.defaultDocumentId;
		const auto id = chosen.id.documentId
			? chosen.id.documentId
			: fallback;
		if (id) {
			state->pending = id;
			preview->update();
			body->update();
			for (const auto child : body->findChildren<QWidget*>()) {
				child->update();
			}
		}
	}, box->lifetime());

	const auto refresh = [=] {
		if (state->saving) {
			return;
		}
		if (kind() != state->kind) {
			state->loaded = false;
			fill();
		} else if (state->kind == 2 && !edited()) {
			state->pending = current().documentId;
			if (state->field
				&& state->field->getLastText().trimmed()
					!= current().text.trimmed()) {
				state->field->setText(current().text);
			}
		}
		preview->update();
	};
	BadgesUpdated() | rpl::on_next(refresh, box->lifetime());
	SyncBadgesNow(crl::guard(box, refresh));
	RefreshAllowedFrames(session);
}

void FrameBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	const auto user = session->user();
	const auto selfId = SelfId(session);
	SetupBox(box);
	box->setTitle(tr::lng_sg_frame_title());
	box->setWidth(st::boxWideWidth);
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });

	struct State {
		QString pending;
		FrameParams::Values params;
		bool loaded = false;
		bool saving = false;
		std::optional<bool> builtAvailable;
		QString builtStyle;
		Ui::PeerUserpicView userpic;
		rpl::event_stream<> changes;
	};
	const auto state = box->lifetime().make_state<State>();
	const auto available = [=] {
		return CanUseFrames(session);
	};
	const auto currentStyle = [=] {
		const auto frame = UserAvatarFrame(selfId);
		return frame ? frame->style : QString();
	};
	const auto currentParams = [=] {
		const auto frame = UserAvatarFrame(selfId);
		return frame ? frame->params : FrameParams::Values();
	};
	const auto pendingFrame = [=]() -> std::optional<AvatarFrame> {
		if (state->pending.isEmpty()) {
			return std::nullopt;
		}
		return AvatarFrame{ state->pending, state->params };
	};
	const auto edited = [=] {
		if (!available()) {
			return false;
		}
		const auto style = currentStyle();
		if (state->pending != style) {
			return true;
		}
		return !style.isEmpty()
			&& (FrameParams::NonDefault(style, state->params)
				!= FrameParams::NonDefault(style, currentParams()));
	};
	const auto canSave = [=] {
		return !state->saving && edited();
	};
	const auto setPending = [=](QString style, FrameParams::Values params) {
		state->pending = style;
		state->params = FrameParams::Sanitize(style, params);
	};
	const auto load = [=] {
		if (!available()) {
			setPending(QString(), {});
			state->loaded = false;
		} else if (!state->loaded) {
			state->loaded = true;
			const auto style = currentStyle();
			setPending(
				style,
				style.isEmpty() ? FrameParams::Values() : currentParams());
		}
	};
	const auto paramValue = [=](const QString &key) {
		const auto i = state->params.constFind(key);
		return (i != state->params.cend())
			? i.value()
			: FrameParams::DefaultValue(state->pending, key);
	};
	const auto toggled = [=](const QString &key) {
		return paramValue(key) != u"0"_q;
	};
	const auto setParam = [=](const QString &key, const QString &value) {
		if (state->pending.isEmpty()) {
			return;
		}
		state->params.insert(key, value);
		state->changes.fire({});
	};

	const auto container = box->verticalLayout();

	// Preview: the own avatar with the frame being chosen.
	const auto previewCard = AddCard(container, 0, 6);
	const auto preview = previewCard->add(
		object_ptr<Ui::RpWidget>(previewCard),
		QMargins());
	const auto previewHeight = st::sgFramePreviewTop
		+ st::sgFramePreviewUserpic
		+ 2 * st::sgFramePreviewRoom
		+ st::sgFramePreviewNameSkip
		+ st::sgFramePreviewNameFont->height
		+ st::sgFramePreviewLineSkip
		+ st::normalFont->height
		+ st::sgFramePreviewBottom;
	preview->resize(st::boxWideWidth, previewHeight);
	preview->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(preview);
		auto hq = PainterHighQualityEnabler(p);
		const auto size = st::sgFramePreviewUserpic;
		const auto left = (preview->width() - size) / 2;
		const auto top = st::sgFramePreviewTop + st::sgFramePreviewRoom;
		user->paintUserpic(p, state->userpic, left, top, size);
		if (const auto frame = pendingFrame()) {
			PaintAvatarFrame(
				p,
				*frame,
				QRectF(left, top, size, size),
				QRectF(
					left - st::sgFramePreviewRoom,
					top - st::sgFramePreviewRoom,
					size + 2 * st::sgFramePreviewRoom,
					size + 2 * st::sgFramePreviewRoom),
				1.,
				AvatarFramesAnimated(),
				AvatarFramesLight());
		}
		const auto width = preview->width()
			- st::boxRowPadding.left()
			- st::boxRowPadding.right();
		auto y = top + size + st::sgFramePreviewRoom
			+ st::sgFramePreviewNameSkip;
		const auto &nameFont = st::sgFramePreviewNameFont;
		p.setFont(nameFont);
		p.setPen(st::windowBoldFg);
		p.drawText(
			QRect(st::boxRowPadding.left(), y, width, nameFont->height),
			Qt::AlignHCenter | Qt::AlignTop,
			nameFont->elided(user->name(), width));
		y += nameFont->height + st::sgFramePreviewLineSkip;
		p.setFont(st::normalFont);
		p.setPen(st::windowSubTextFg);
		p.drawText(
			QRect(st::boxRowPadding.left(), y, width, st::normalFont->height),
			Qt::AlignHCenter | Qt::AlignTop,
			AvatarFrameName(state->pending));
	}, preview->lifetime());

	const auto styleCard = AddCard(container);
	AddCardTitle(styleCard, tr::lng_sg_frame_style());

	// The style row: none, then the styles, on the own avatar.
	const auto picker = styleCard->add(
		object_ptr<Ui::RpWidget>(styleCard),
		QMargins(0, 0, 0, 6));
	picker->resize(st::boxWideWidth, st::sgFramePickerHeight);
	const auto styles = [] {
		auto result = QStringList{ QString() };
		result.append(FrameParams::Styles());
		return result;
	}();
	const auto itemRect = [=](int index) {
		const auto padding = st::sgFramePickerPadding;
		const auto inner = picker->width() - padding.left() - padding.right();
		const auto count = int(styles.size());
		const auto width = inner / count;
		return QRect(
			padding.left() + index * width,
			0,
			width,
			picker->height());
	};
	const auto select = [=](const QString &style) {
		if (!available()) {
			if (!style.isEmpty()) {
				box->showToast(tr::lng_sg_frame_need_badge(tr::now));
			}
			return;
		} else if (!style.isEmpty() && !CanUseFrame(session, style)) {
			box->showToast(tr::lng_sg_frame_not_included(tr::now));
			return;
		} else if (style == state->pending) {
			return;
		}
		// Back to the saved style restores its saved settings.
		setPending(
			style,
			(!style.isEmpty() && style == currentStyle())
				? currentParams()
				: FrameParams::Values());
		state->changes.fire({});
	};
	picker->setCursor(style::cur_pointer);
	picker->events(
	) | rpl::filter([](not_null<QEvent*> e) {
		return e->type() == QEvent::MouseButtonRelease;
	}) | rpl::on_next([=](not_null<QEvent*> e) {
		const auto position = static_cast<QMouseEvent*>(e.get())->pos();
		for (auto i = 0; i != int(styles.size()); ++i) {
			if (itemRect(i).contains(position)) {
				select(styles[i]);
				break;
			}
		}
	}, picker->lifetime());
	picker->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(picker);
		auto hq = PainterHighQualityEnabler(p);
		const auto size = st::sgFramePickerUserpic;
		const auto animated = AvatarFramesAnimated();
		const auto light = AvatarFramesLight();
		for (auto i = 0; i != int(styles.size()); ++i) {
			const auto &style = styles[i];
			const auto rect = itemRect(i);
			const auto selected = (style == state->pending);
			const auto allowed = style.isEmpty()
				|| CanUseFrame(session, style);
			const auto alpha = allowed
				? 1.
				: st::sgFramePickerLockedOpacity;
			const auto left = rect.x() + (rect.width() - size) / 2;
			const auto top = st::sgFramePickerUserpicTop;
			p.setOpacity(alpha);
			user->paintUserpic(p, state->userpic, left, top, size);
			p.setOpacity(1.);
			if (!style.isEmpty()) {
				const auto frame = AvatarFrame{
					style,
					selected ? state->params : FrameParams::Values(),
				};
				PaintAvatarFrame(
					p,
					frame,
					QRectF(left, top, size, size),
					QRectF(rect),
					alpha,
					animated,
					light);
			}
			const auto shift = st::sgFramePickerCheckShift;
			const auto markCenter = QPointF(
				left + size - shift,
				top + size - shift);
			if (selected) {
				PaintMark(p, markCenter, true);
			} else if (!allowed) {
				PaintMark(p, markCenter, false);
			}
			const auto &font = selected
				? st::sgFramePickerFontSelected
				: st::sgFramePickerFont;
			p.setFont(font);
			p.setPen(selected ? st::windowActiveTextFg : st::windowSubTextFg);
			p.drawText(
				QRect(
					rect.x(),
					top + size + st::sgFramePickerLabelSkip,
					rect.width(),
					font->height),
				Qt::AlignHCenter | Qt::AlignTop,
				font->elided(AvatarFrameName(style), rect.width()));
		}
	}, picker->lifetime());

	session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		preview->update();
		picker->update();
	}, box->lifetime());

	const auto body = container->add(
		object_ptr<Ui::VerticalLayout>(container));

	const auto save = [=] {
		if (!canSave()) {
			return;
		} else if (ReadBadgesConfig(session).bot.isEmpty()) {
			box->showToast(tr::lng_sg_badge_unavailable(tr::now));
			return;
		}
		state->saving = true;
		state->changes.fire({});
		ApplyAvatarFrame(session, pendingFrame(), crl::guard(box, [=](
				QString result) {
			state->saving = false;
			if (result == u"ok"_q) {
				box->showToast(tr::lng_sg_frame_saved(tr::now));
			} else {
				const auto reason = result.isEmpty()
					? tr::lng_sg_badge_unknown_error(tr::now)
					: result.contains(u"a frame comes with a badge"_q)
					? tr::lng_sg_frame_need_badge(tr::now)
					: result.contains(u"this frame is not included"_q)
					? tr::lng_sg_frame_not_included(tr::now)
					: result;
				box->showToast(tr::lng_sg_badge_save_error(
					tr::now,
					lt_reason,
					reason));
			}
			state->changes.fire({});
		}));
	};

	const auto addChips = [=](
			not_null<Ui::VerticalLayout*> to,
			const QString &key,
			std::vector<ChipsRow::Option> options) {
		AddCardTitle(to, rpl::single(KeyName(key)));
		to->add(
			object_ptr<ChipsRow>(
				to,
				std::move(options),
				state->changes.events_starting_with(
					{}
				) | rpl::map([=] {
					return paramValue(key);
				}),
				[=](QString value) { setParam(key, value); }),
			kCardRowMargins);
	};
	const auto valueOptions = [=](
			const QString &style,
			const QString &key) {
		auto result = std::vector<ChipsRow::Option>();
		const auto values = FrameParams::AllowedValues(style, key);
		if (!values) {
			return result;
		}
		for (auto i = 0; i != int(values->size()); ++i) {
			const auto &value = (*values)[i];
			auto dot = std::optional<QColor>();
			if (key == u"gem"_q) {
				const auto color = AvatarFrameGemColor(i);
				if (color.alpha()) {
					dot = color;
				}
			} else if (key != u"gempos"_q) {
				dot = AvatarFramePaletteColor(style, i);
			}
			result.push_back({
				.value = value,
				.label = OptionName(value),
				.dot = dot,
			});
		}
		return result;
	};

	const auto fill = [=] {
		const auto canUse = available();
		state->builtAvailable = canUse;
		state->builtStyle = state->pending;
		body->clear();
		const auto style = state->pending;
		if (canUse) {
			if (!style.isEmpty()) {
				const auto settings = AddCard(body);
				const auto paletteKey = FrameParams::PaletteKey(style);
				addChips(
					settings,
					paletteKey,
					valueOptions(style, paletteKey));
				if (style == u"solitaire"_q) {
					addChips(
						settings,
						u"gem"_q,
						valueOptions(style, u"gem"_q));
					addChips(
						settings,
						u"gempos"_q,
						valueOptions(style, u"gempos"_q));
				}
				auto toggles = FrameParams::ToggleKeys(style);
				toggles.push_back(u"anim"_q);
				for (const auto &key : toggles) {
					const auto title = (key == u"anim"_q)
						? tr::lng_sg_frame_animation(tr::now)
						: KeyName(key);
					const auto button = Settings::AddButtonWithIcon(
						settings,
						rpl::single(title),
						st::settingsButtonNoIcon);
					button->toggleOn(
						state->changes.events_starting_with(
							{}
						) | rpl::map([=] {
							return toggled(key);
						}) | rpl::distinct_until_changed());
					button->toggledChanges(
					) | rpl::filter([=](bool value) {
						return value != toggled(key);
					}) | rpl::on_next([=](bool value) {
						setParam(key, value ? u"1"_q : u"0"_q);
					}, button->lifetime());
				}

				const auto motion = AddCard(body);
				auto speeds = std::vector<ChipsRow::Option>();
				for (const auto &speed : FrameParams::Speeds()) {
					speeds.push_back({
						.value = speed,
						.label = speed + QChar(0x00D7),
					});
				}
				AddCardTitle(motion, tr::lng_sg_frame_speed());
				motion->add(
					object_ptr<ChipsRow>(
						motion,
						std::move(speeds),
						state->changes.events_starting_with(
							{}
						) | rpl::map([=] {
							return paramValue(u"speed"_q);
						}),
						[=](QString value) {
							setParam(u"speed"_q, value);
						}),
					kCardRowMargins);

				AddCardTitle(motion, tr::lng_sg_frame_glow());
				auto slider = Settings::MakeSliderWithLabel(
					motion,
					st::settingsScale,
					st::settingsScaleLabel,
					st::normalFont->spacew * 2,
					st::settingsScaleLabel.style.font->width(u"100%"_q),
					true);
				const auto label = slider.label;
				motion->add(
					std::move(slider.widget),
					kCardSliderMargins);
				const auto updateLabel = [=](int value) {
					label->setText(QString::number(value) + '%');
				};
				const auto glow = paramValue(u"glow"_q).toInt();
				slider.slider->setPseudoDiscrete(
					101,
					[](int index) { return index; },
					glow,
					[=](int value) {
						updateLabel(value);
						setParam(u"glow"_q, QString::number(value));
					},
					[=](int value) {
						setParam(u"glow"_q, QString::number(value));
					});
				updateLabel(glow);
			}
			AddCardNote(body, tr::lng_sg_frame_info());
			const auto saveButton = AddMainButton(
				body,
				tr::lng_sg_frame_save(),
				save,
				4);
			state->changes.events_starting_with(
				{}
			) | rpl::on_next([=] {
				// A pill that has nothing to save looks disabled.
				const auto active = canSave();
				saveButton->setBrushOverride(active
					? std::nullopt
					: std::make_optional(QBrush(st::windowBgOver->c)));
				saveButton->setTextFgOverride(active
					? std::nullopt
					: std::make_optional(st::windowSubTextFg->c));
			}, saveButton->lifetime());
		} else {
			const auto config = ReadBadgesConfig(session);
			AddCardNote(
				body,
				(config.apiUrl.isEmpty() && config.bot.isEmpty())
					? tr::lng_sg_badge_unavailable()
					: tr::lng_sg_frame_need_badge_info());
			if (!config.shopUrl.isEmpty()) {
				AddMainButton(body, tr::lng_sg_badge_buy(), [=] {
					ShowSupporterBox(controller);
				}, 4);
			}
		}
		body->resizeToWidth(container->width());
	};

	state->changes.events(
	) | rpl::on_next([=] {
		preview->update();
		picker->update();
		if (state->builtAvailable != available()
			|| state->builtStyle != state->pending) {
			fill();
		}
	}, box->lifetime());

	const auto reload = [=] {
		if (state->saving) {
			return;
		}
		if (state->loaded && !edited()) {
			state->loaded = false;
		}
		load();
		state->changes.fire({});
	};
	load();
	fill();
	BadgesUpdated() | rpl::on_next(reload, box->lifetime());
	SyncBadgesNow(crl::guard(box, reload));
	RefreshAllowedFrames(session, crl::guard(box, reload));
}

void BannerBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	const auto selfId = SelfId(session);
	SetupBox(box);
	box->setTitle(tr::lng_sg_banner_title());
	box->setWidth(st::boxWideWidth);

	struct State {
		bool busy = false;
	};
	const auto state = box->lifetime().make_state<State>();

	const auto container = box->verticalLayout();
	const auto previewCard = AddCard(container, 0, 6);
	const auto preview = previewCard->add(
		object_ptr<Ui::RpWidget>(previewCard),
		QMargins());
	const auto padding = QMargins(12, 8, 12, 8);
	preview->resize(
		st::boxWideWidth,
		padding.top() + st::sgBannerPreviewHeight + padding.bottom());
	preview->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(preview);
		auto hq = PainterHighQualityEnabler(p);
		const auto target = QRect(
			padding.left(),
			padding.top(),
			preview->width() - padding.left() - padding.right(),
			st::sgBannerPreviewHeight);
		auto path = QPainterPath();
		const auto radius = Mokagram::BoxParts::kCardRadius - 8;
		path.addRoundedRect(target, radius, radius);
		const auto image = UserBannerImage(selfId);
		p.setClipPath(path);
		if (image.isNull()) {
			p.fillRect(target, st::boxDividerBg);
			p.setClipping(false);
			p.setFont(st::normalFont);
			p.setPen(st::windowSubTextFg);
			p.drawText(
				target,
				Qt::AlignCenter,
				BannerOfUser(selfId)
					? tr::lng_sg_banner_current(tr::now)
					: tr::lng_sg_banner_none(tr::now));
		} else {
			const auto scale = std::max(
				target.width() / double(image.width()),
				target.height() / double(image.height()));
			const auto width = image.width() * scale;
			const auto height = image.height() * scale;
			p.drawImage(
				QRectF(
					target.x() + (target.width() - width) / 2.,
					target.y() + (target.height() - height) / 2.,
					width,
					height),
				image);
		}
	}, preview->lifetime());
	BadgesUpdated() | rpl::on_next([=] {
		preview->update();
	}, preview->lifetime());
	AddCardNote(container, tr::lng_sg_banner_info());

	const auto finish = [=](QString error, bool removed) {
		state->busy = false;
		if (error.isEmpty()) {
			box->showToast(removed
				? tr::lng_sg_banner_removed(tr::now)
				: tr::lng_sg_banner_saved(tr::now));
		} else if (error == u"photo"_q || error == u"video"_q) {
			box->showToast(tr::lng_sg_banner_too_large(tr::now));
		} else if (error.contains(u"error: a banner comes"_q)) {
			box->showToast(tr::lng_sg_banner_need_badge(tr::now));
		} else {
			box->showToast(tr::lng_sg_banner_failed(
				tr::now,
				lt_reason,
				error));
		}
	};
	const auto choose = [=] {
		if (state->busy) {
			return;
		} else if (!BadgeOfUser(selfId)) {
			box->showToast(tr::lng_sg_banner_need_badge(tr::now));
			return;
		}
		const auto filter = u"Pictures and videos (*.jpg *.jpeg *.png *.webp *.mp4 *.m4v);;"_q
			+ FileDialog::AllFilesFilter();
		FileDialog::GetOpenPath(
			box.get(),
			tr::lng_sg_banner_choose(tr::now),
			filter,
			crl::guard(box, [=](const FileDialog::OpenResult &result) {
				if (result.paths.size() != 1 || state->busy) {
					return;
				}
				state->busy = true;
				box->showToast(tr::lng_sg_banner_uploading(tr::now));
				UploadBanner(
					session,
					result.paths.front(),
					crl::guard(box, [=](QString error) {
						finish(error, false);
					}));
			}));
	};
	const auto remove = [=] {
		if (state->busy || !BannerOfUser(selfId)) {
			return;
		}
		state->busy = true;
		RemoveBanner(session, crl::guard(box, [=](QString error) {
			finish(error, true);
		}));
	};
	box->addButton(
		tr::lng_sg_banner_choose(),
		choose,
		Mokagram::BoxParts::PillActive());
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	box->addLeftButton(
		tr::lng_sg_banner_remove(),
		remove,
		Mokagram::BoxParts::PillAttention());

	// Whether the banner and the badge changed on the server.
	SyncBadgesNow(crl::guard(box, [=] {
		preview->update();
	}));
}

} // namespace

QString AvatarFrameName(const QString &style) {
	if (style == u"inferno"_q) {
		return tr::lng_sg_frame_inferno(tr::now);
	} else if (style == u"pixel"_q) {
		return tr::lng_sg_frame_pixel(tr::now);
	} else if (style == u"solitaire"_q) {
		return tr::lng_sg_frame_solitaire(tr::now);
	} else if (style == u"cosmos"_q) {
		return tr::lng_sg_frame_cosmos(tr::now);
	} else if (style == u"frost"_q) {
		return tr::lng_sg_frame_frost(tr::now);
	} else if (style == u"neko"_q) {
		return tr::lng_sg_frame_neko(tr::now);
	} else if (style == u"angel"_q) {
		return tr::lng_sg_frame_angel(tr::now);
	} else if (style == u"devil"_q) {
		return tr::lng_sg_frame_devil(tr::now);
	} else if (style.isEmpty()) {
		return tr::lng_sg_frame_none(tr::now);
	}
	return style;
}

rpl::producer<QString> SelfAvatarFrameNameValue(
		not_null<Main::Session*> session) {
	const auto selfId = SelfId(session);
	return rpl::single(
		rpl::empty
	) | rpl::then(
		BadgesUpdated()
	) | rpl::map([=] {
		const auto frame = UserAvatarFrame(selfId);
		return AvatarFrameName(
			(frame && CanUseFrames(session)) ? frame->style : QString());
	}) | rpl::distinct_until_changed();
}

void ShowBadgeBox(not_null<Window::SessionController*> controller) {
	controller->show(Box(BadgeBox, controller));
}

void ShowFrameBox(not_null<Window::SessionController*> controller) {
	controller->show(Box(FrameBox, controller));
}

void ShowBannerBox(not_null<Window::SessionController*> controller) {
	controller->show(Box(BannerBox, controller));
}

void ShowSupporterBox(
		not_null<Window::SessionController*> controller,
		bool fromBadgeBox) {
	controller->show(Box(SupporterBox, controller, fromBadgeBox));
}

} // namespace StaticGram
