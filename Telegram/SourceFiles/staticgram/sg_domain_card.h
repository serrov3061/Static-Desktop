/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"

// The .ton domain card (Android ui/Wallet/SGDomainCardDrawable and
// SGTonDns.mediaUrl): the server render of a minted domain
// (fragment.staticgram.top/api/nft/item/domain/<name>/media.png), with the
// local vector render of the same flat card meanwhile and for the domains
// that are not minted yet.
namespace StaticGram::Domains {

// The server card of "alice" / "alice.ton".
[[nodiscard]] QString CardMediaUrl(const QString &name);

class CardView final : public Ui::RpWidget {
public:
	// radiusFraction: the corner radius as a fraction of the side (0 for a
	// square card, Android setRadiusFraction).
	CardView(
		QWidget *parent,
		const QString &name,
		float64 radiusFraction,
		bool server = false);

	void setName(const QString &name);
	// Show the server render once it is loaded (minted domains).
	void setServer(bool server);

private:
	void paintEvent(QPaintEvent *e) override;

	[[nodiscard]] QRect cardRect() const;

	QString _name; // "alice.ton"
	QString _short; // "alice"
	float64 _radius = 0.;
	bool _server = false;
	QImage _local;
	QImage _remote;

};

} // namespace StaticGram::Domains
