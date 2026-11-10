/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/channel_statistics/earn/earn_format.h"

#include <QtCore/QLocale>

namespace Info::ChannelEarn {
namespace {

constexpr auto kMinorPartLength = 9;
constexpr auto kMaxChoppedZero = kMinorPartLength - 2;
constexpr auto kZero = QChar('0');
const auto DecimalPoint = QString() + QLocale().decimalPoint();

using EarnInt = Data::EarnInt;

} // namespace

QString MajorPart(EarnInt value) {
	const auto string = QString::number(value);
	const auto diff = int(string.size()) - kMinorPartLength;
	return (diff <= 0) ? QString(kZero) : string.mid(0, diff);
}

QString MajorPart(CreditsAmount value) {
	return QString::number(int64(value.value()));
}

QString MinorPart(EarnInt value) {
	if (!value) {
		return DecimalPoint + kZero + kZero;
	}
	const auto string = QString::number(value);
	const auto diff = int(string.size()) - kMinorPartLength;
	const auto result = (diff < 0)
		? DecimalPoint + u"%1"_q.arg(0, std::abs(diff), 10, kZero) + string
		: DecimalPoint + string.mid(diff);
	const auto begin = (result.constData());
	const auto end = (begin + result.size());
	auto ch = end - 1;
	auto zeroCount = 0;
	while (ch != begin) {
		if (((*ch) == kZero) && (zeroCount < kMaxChoppedZero)) {
			zeroCount++;
		} else {
			break;
		}
		ch--;
	}
	return result.chopped(zeroCount);
}

QString MinorPart(CreditsAmount value) {
	// StaticGram: two decimals as before, but an amount under one shows
	// its significant digits (Android SGAmounts): 0.0005, 0.000000001,
	// instead of a wrong "0.00".
	const auto nano = std::abs(value.nano()) % 1'000'000'000;
	const auto digits = QString::number(nano).rightJustified(
		kMinorPartLength,
		kZero);
	auto keep = 2;
	if (!value.whole() && nano) {
		auto first = 0;
		while (first < kMinorPartLength && digits[first] == kZero) {
			++first;
		}
		keep = std::clamp(first + 2, 2, kMinorPartLength);
	}
	auto part = digits.left(keep);
	while (part.size() > 2 && part.endsWith(kZero)) {
		part.chop(1);
	}
	return DecimalPoint + part;
}

QString ToUsd(
		Data::EarnInt value,
		float64 rate,
		int afterFloat) {
	return ToUsd(CreditsAmount(value), rate, afterFloat);
}

QString ToUsd(
		CreditsAmount value,
		float64 rate,
		int afterFloat) {
	constexpr auto kApproximately = QChar(0x2248);

	return QString(kApproximately)
		+ QChar('$')
		+ QLocale().toString(
			value.value() * rate,
			'f',
			afterFloat ? afterFloat : 2);
}

} // namespace Info::ChannelEarn
