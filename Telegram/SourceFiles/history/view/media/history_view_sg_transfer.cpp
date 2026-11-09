/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/media/history_view_sg_transfer.h"

#include "base/unixtime.h"
#include "core/click_handler_types.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/view/media/history_view_media_generic.h"
#include "history/view/history_view_cursor_state.h"
#include "history/view/history_view_element.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "info/channel_statistics/earn/earn_icons.h"
#include "info/peer_gifts/info_peer_gifts_common.h"
#include "lang/lang_keys.h"
#include "lang/lang_tag.h"
#include "main/session/session_show.h"
#include "main/main_session.h"
#include "staticgram/sg_box_parts.h"
#include "ui/controls/ton_common.h"
#include "ui/effects/credits_graphics.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "wallet/wallet_amount_painter.h"
#include "window/window_session_controller.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>
#include <QtGui/QPainterPath>

#include "styles/style_chat.h"
#include "styles/style_layers.h"
#include "styles/style_polls.h"
#include "styles/style_sg_ui.h"
#include "styles/style_wallet.h"

namespace HistoryView {
namespace {

// Android WalletActionLayout: the golden Stars card.
[[nodiscard]] QColor StarsCardDark() {
	return QColor(0xa8, 0x5a, 0x00);
}

[[nodiscard]] QColor StarsCardLight() {
	return QColor(0xe0, 0x8f, 0x0b);
}

[[nodiscard]] QColor StarsUnitFg() {
	return QColor(0xff, 0xe0, 0x8a);
}

[[nodiscard]] QColor StarsRibbonLight(bool outgoing) {
	return outgoing ? QColor(0xff, 0xc2, 0x33) : QColor(0xff, 0xd4, 0x5c);
}

[[nodiscard]] QColor StarsRibbonDark(bool outgoing) {
	return outgoing ? QColor(0xb8, 0x64, 0x00) : QColor(0xe0, 0x8f, 0x0b);
}

constexpr auto kGrantPrefix = "sggrant:";

[[nodiscard]] bool TransferOutgoing(
		not_null<HistoryItem*> item,
		not_null<const HistoryServiceGramTransfer*> transfer) {
	return item->out() || transfer->sgOutgoing;
}

// The name on the card: the other side of the chat, the giveaway channel
// for a prize, the service account itself for its top-ups and grants.
[[nodiscard]] QString CounterpartyName(
		not_null<HistoryItem*> item,
		not_null<const HistoryServiceGramTransfer*> transfer) {
	if (transfer->sgPrize && transfer->sgCardPeer) {
		const auto peer = item->history()->owner().peer(
			transfer->sgCardPeer);
		if (!peer->name().isEmpty()) {
			return peer->name();
		}
	}
	if (item->history()->peer->isNotificationsUser()) {
		return u"StaticGram"_q;
	}
	const auto peer = TransferOutgoing(item, transfer)
		? item->history()->peer.get()
		: item->from().get();
	return TextUtilities::SingleLine(peer->name());
}

[[nodiscard]] Wallet::AmountStyle StarsAmountStyle() {
	return {
		.big = st::walletCardBalanceMajorLabel.style.font,
		.small = st::walletCardBalanceMinorLabel.style.font,
		.tickerSkip = st::walletCardTickerSkip,
		.hinted = true,
	};
}

[[nodiscard]] Wallet::AmountParts StarsAmount(int64 stars, bool outgoing) {
	stars = std::abs(stars);
	return {
		.whole = (outgoing ? QChar(0x2212) : QChar('+'))
			+ Lang::FormatCountDecimal(stars),
		.ticker = tr::lng_sg_stars_ticker(tr::now, lt_count, stars),
	};
}

[[nodiscard]] ClickHandlerPtr DetailsLink(FullMsgId itemId) {
	return std::make_shared<LambdaClickHandler>([=](ClickContext context) {
		const auto my = context.other.value<ClickHandlerContext>();
		const auto controller = my.sessionWindow.get();
		if (context.button != Qt::LeftButton || !controller) {
			return;
		}
		if (const auto item = controller->session().data().message(itemId)) {
			ShowSgTransferDetails(controller->uiShow(), item);
		}
	});
}

class SgStarsCardPart final : public MediaGenericPart {
public:
	SgStarsCardPart(
		not_null<HistoryItem*> item,
		not_null<const HistoryServiceGramTransfer*> transfer);

	void draw(
		Painter &p,
		not_null<const MediaGeneric*> owner,
		const PaintContext &context,
		int outerWidth) const override;
	TextState textState(
		QPoint point,
		StateRequest request,
		int outerWidth) const override;

	QSize countOptimalSize() override;
	QSize countCurrentSize(int newWidth) override;

private:
	int resolveLayout(int outerWidth);
	void validateImages() const;

	const ClickHandlerPtr _link;
	const QString _identity;
	const QString _ribbon;
	const bool _outgoing = false;
	Wallet::AmountPainter _amount;

	QRect _card;
	QString _identityElided;
	int _markTop = 0;
	int _amountTop = 0;
	int _identityTop = 0;

	mutable QImage _star;
	mutable QImage _badge;
	mutable int _ratio = 0;

};

SgStarsCardPart::SgStarsCardPart(
	not_null<HistoryItem*> item,
	not_null<const HistoryServiceGramTransfer*> transfer)
: _link(DetailsLink(item->fullId()))
, _identity(CounterpartyName(item, transfer).toUpper())
, _ribbon(TransferOutgoing(item, transfer)
	? tr::lng_action_gram_transfer_sent_tag(tr::now)
	: tr::lng_action_gram_transfer_received_tag(tr::now))
, _outgoing(TransferOutgoing(item, transfer))
, _amount(StarsAmountStyle(), StarsAmount(transfer->amount, _outgoing)) {
}

void SgStarsCardPart::validateImages() const {
	const auto ratio = style::DevicePixelRatio();
	if (_ratio == ratio && !_star.isNull() && !_badge.isNull()) {
		return;
	}
	_ratio = ratio;
	_star = Ui::GenerateStars(st::walletChatCardMarkSize, 1);
	_badge = Info::PeerGifts::ValidateRotatedBadge({
		.text = _ribbon,
		.bg1 = StarsRibbonLight(_outgoing),
		.bg2 = StarsRibbonDark(_outgoing),
		.fg = QColor(255, 255, 255),
		.gradient = true,
	}, st::chatUniqueGiftBadgePadding);
}

void SgStarsCardPart::draw(
		Painter &p,
		not_null<const MediaGeneric*> owner,
		const PaintContext &context,
		int outerWidth) const {
	if (_card.isEmpty()) {
		return;
	}
	validateImages();
	p.save();
	auto hq = PainterHighQualityEnabler(p);
	const auto radius = std::max(
		st::msgServiceGiftBoxRadius - st::chatUniqueGiftBorder,
		1);
	auto clip = QPainterPath();
	clip.addRoundedRect(QRectF(_card), radius, radius);
	p.setClipPath(clip, Qt::IntersectClip);

	// A sweep of dark and light gold, as the Android card.
	auto gradient = QConicalGradient(QPointF(_card.center()), 45.);
	gradient.setStops({
		{ 0., StarsCardDark() },
		{ 0.25, StarsCardLight() },
		{ 0.5, StarsCardDark() },
		{ 0.75, StarsCardLight() },
		{ 1., StarsCardDark() },
	});
	p.fillRect(_card, gradient);

	p.translate(_card.topLeft());
	const auto cardWidth = _card.width();
	const auto starSize = _star.width() / _star.devicePixelRatio();
	p.drawImage(QPointF((cardWidth - starSize) / 2., _markTop), _star);

	_amount.paint(
		p,
		QPointF((cardWidth - _amount.size().width()) / 2., _amountTop),
		Wallet::AmountColors{
			.digits = QColor(255, 255, 255),
			.ticker = StarsUnitFg(),
		});

	const auto &font = st::walletCardNameFont;
	p.setPen(StarsUnitFg());
	p.setFont(font);
	p.drawText(
		(cardWidth - font->width(_identityElided)) / 2,
		_identityTop + font->ascent,
		_identityElided);

	p.drawImage(
		QPointF(cardWidth - _badge.width() / _badge.devicePixelRatio(), 0.),
		_badge);
	p.restore();
}

TextState SgStarsCardPart::textState(
		QPoint point,
		StateRequest request,
		int outerWidth) const {
	auto result = TextState();
	if (_card.contains(point)) {
		result.link = _link;
	}
	return result;
}

QSize SgStarsCardPart::countOptimalSize() {
	const auto width = st::chatUniqueGiftMaxWidth;
	return { width, resolveLayout(width) };
}

QSize SgStarsCardPart::countCurrentSize(int newWidth) {
	return { newWidth, resolveLayout(newWidth) };
}

int SgStarsCardPart::resolveLayout(int outerWidth) {
	const auto border = st::chatUniqueGiftBorder;
	const auto cardWidth = std::max(outerWidth - 2 * border, 0);
	const auto available = std::max(
		cardWidth - 2 * st::walletCardContentLeft,
		1);
	_markTop = st::walletChatCardMarkTop;
	_amount.setAvailableWidth(available);
	const auto amountHeight = int(std::ceil(_amount.size().height()));
	_amountTop = _markTop
		+ st::walletChatCardMarkSize
		+ st::walletChatCardAmountSkip;
	const auto &font = st::walletCardNameFont;
	_identityElided = font->elided(_identity, available);
	_identityTop = _amountTop + amountHeight + st::walletChatCardNameSkip;
	const auto cardHeight = _identityTop
		+ font->height
		+ st::walletChatCardAddressSkip
		+ st::walletChatCardBottom;
	_card = QRect(border, border, cardWidth, cardHeight);
	return cardHeight + 2 * border;
}

void AddDetailsRow(
		not_null<Ui::VerticalLayout*> card,
		rpl::producer<QString> caption,
		TextWithEntities value,
		Fn<void()> copy = nullptr) {
	const auto &padding = st::sgTransferRowPadding;
	card->add(
		object_ptr<Ui::FlatLabel>(
			card,
			std::move(caption),
			st::sgTransferCaption),
		QMargins(padding.left(), padding.top(), padding.right(), 0));
	const auto label = card->add(
		object_ptr<Ui::FlatLabel>(
			card,
			rpl::single(std::move(value)),
			st::sgTransferValue),
		QMargins(
			padding.left(),
			st::sgTransferRowSkip,
			padding.right(),
			padding.bottom()));
	label->setBreakEverywhere(true);
	if (copy) {
		label->setClickHandlerFilter([=](const auto &...) {
			copy();
			return false;
		});
	} else {
		label->setSelectable(true);
	}
}

void SgTransferDetailsBox(
		not_null<Ui::GenericBox*> box,
		std::shared_ptr<Main::SessionShow> show,
		not_null<HistoryItem*> item) {
	using namespace StaticGram::BoxParts;
	const auto transfer = item->Get<HistoryServiceGramTransfer>();
	Assert(transfer != nullptr);

	const auto stars = transfer->sgStars;
	const auto outgoing = TransferOutgoing(item, transfer);
	const auto rawId = transfer->transactionId;
	const auto grant = IsSgGrantTransactionId(rawId);
	const auto id = grant
		? rawId.mid(QString::fromLatin1(kGrantPrefix).size())
		: rawId;

	SetupBox(box);
	box->setWidth(st::boxWideWidth);
	box->setNoContentMargin(true);

	const auto content = box->verticalLayout();
	const auto iconSize = st::sgTransferIconSize;
	const auto icon = content->add(
		object_ptr<Ui::RpWidget>(content),
		st::sgTransferIconPadding,
		style::al_top);
	icon->resize(iconSize, iconSize);
	const auto image = stars
		? Ui::GenerateStars(iconSize, 1)
		: Ui::Earn::IconCurrencyMono(iconSize, st::windowActiveTextFg->c);
	icon->paintOn([=](QPainter &p) {
		const auto ratio = image.devicePixelRatio();
		const auto size = QSizeF(image.size()) / ratio;
		p.drawImage(
			QRectF(
				(icon->width() - size.width()) / 2.,
				(icon->height() - size.height()) / 2.,
				size.width(),
				size.height()),
			image);
	});

	const auto amount = stars
		? tr::lng_sg_stars_amount(
			tr::now,
			lt_count_decimal,
			std::abs(transfer->amount))
		: tr::lng_action_gram_transfer_amount(
			tr::now,
			lt_count,
			std::abs(transfer->amount) / float64(Ui::kNanosInOne),
			lt_amount,
			Ui::FormatTonAmount(std::abs(transfer->amount)).full);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			(outgoing ? QString(QChar(0x2212)) : u"+"_q) + amount,
			st::sgTransferAmount),
		QMargins(st::boxRowPadding.left(), 0, st::boxRowPadding.right(), 4),
		style::al_top);
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			(transfer->sgPrize
				? tr::lng_sg_transfer_title_prize()
				: grant
				? tr::lng_sg_transfer_title_grant()
				: stars
				? tr::lng_sg_transfer_title_stars()
				: tr::lng_sg_transfer_title_gram()),
			st::sgTransferSubtitle),
		QMargins(st::boxRowPadding.left(), 0, st::boxRowPadding.right(), 8),
		style::al_top);

	const auto copyText = [=](QString text) {
		return [=] {
			QGuiApplication::clipboard()->setText(text);
			show->showToast(tr::lng_sg_transfer_copied(tr::now));
		};
	};

	const auto card = AddCard(content, 4, 6);
	AddDetailsRow(
		card,
		(outgoing ? tr::lng_sg_transfer_to() : tr::lng_sg_transfer_from()),
		tr::marked(CounterpartyName(item, transfer)));
	AddDetailsRow(
		card,
		tr::lng_sg_transfer_date(),
		tr::marked(langDateTimeFull(base::unixtime::parse(item->date()))));
	if (!transfer->peerAddress.isEmpty()) {
		AddDetailsRow(
			card,
			tr::lng_sg_transfer_address(),
			tr::link(transfer->peerAddress),
			copyText(transfer->peerAddress));
	}
	if (!id.isEmpty() && id != u"0"_q) {
		AddDetailsRow(
			card,
			tr::lng_sg_transfer_id(),
			tr::link(id),
			copyText(id));
	}
	const auto comment = transfer->commentText();
	if (!comment.isEmpty() && !transfer->commentEncrypted) {
		const auto commentCard = AddCard(content, 6, 6);
		AddDetailsRow(
			commentCard,
			tr::lng_sg_transfer_comment(),
			tr::marked(transfer->comment));
	}

	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace

bool IsSgGrantTransactionId(const QString &id) {
	return id.startsWith(QString::fromLatin1(kGrantPrefix));
}

std::unique_ptr<Media> CreateSgStarsTransferMedia(
		not_null<Element*> parent,
		Element *replacing) {
	return std::make_unique<MediaGeneric>(
		parent,
		[parent](
				not_null<MediaGeneric*> media,
				Fn<void(std::unique_ptr<MediaGenericPart>)> push) {
			const auto item = parent->data();
			const auto transfer = item->Get<HistoryServiceGramTransfer>();
			if (!transfer) {
				return;
			}
			push(std::make_unique<SgStarsCardPart>(item, transfer));
			const auto comment = transfer->commentText();
			if (!comment.isEmpty()) {
				const auto skip = st::walletChatCardCommentSkip;
				const auto side = 2 * st::chatUniqueGiftBorder;
				push(std::make_unique<MediaGenericTextPart>(
					tr::marked(comment),
					QMargins(
						side,
						skip,
						side,
						skip + st::chatUniqueGiftBorder)));
			}
		},
		MediaGenericDescriptor{
			.maxWidth = st::chatUniqueGiftMaxWidth,
			.service = true,
			.hideServiceText = false,
		});
}

void ShowSgTransferDetails(
		std::shared_ptr<Main::SessionShow> show,
		not_null<HistoryItem*> item) {
	if (!item->Has<HistoryServiceGramTransfer>()) {
		return;
	}
	show->showBox(Box(SgTransferDetailsBox, show, item));
}

} // namespace HistoryView
