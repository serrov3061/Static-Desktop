/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_moka_features.h"

#include "staticgram/sg_box_parts.h"
#include "mokagram/lang/mokagram_lang.h"
#include "mokagram/settings/mokagram_settings.h"
#include "mokagram/streamer/mokagram_streamer.h"
#include "mokagram/quote/mokagram_quote.h"
#include "mokagram/music/mokagram_motion.h"
#include "mokagram/music/mokagram_soundcloud.h"
#include "settings/settings_common.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "lang/lang_keys.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace StaticGram::MokaFeatures {
namespace {

namespace S = ::Mokagram::Settings;

[[nodiscard]] QString Tr(const char *russian) {
	return QString::fromUtf8(Mokagram::Lang::Tr(russian));
}

[[nodiscard]] QString LagSection() {
	return Tr("Анти-лаг");
}

[[nodiscard]] QString StartSection() {
	return Tr("Запуск и фон");
}

void RegisterLag() {
	const auto lag = [&](S::Entry entry) {
		entry.section = LagSection();
		entry.category = u"look"_q;
		entry.sectionNote = Tr("Клиент следит за подвисаниями интерфейса. "
			"Если их много, он на время отключает тяжёлые анимации, "
			"а когда всё спокойно, возвращает как было.");
		S::Register(std::move(entry));
	};
	lag({
		.key = u"lag.enabled"_q,
		.title = Tr("Анти-лаг"),
		.description = Tr("Автоматически включает экономный режим при подвисаниях."),
		.kind = S::Kind::Bool,
		.defaultValue = true,
	});
	lag({
		.key = u"lag.idle"_q,
		.title = Tr("Экономия в фоне"),
		.description = Tr("Когда окно не в фокусе, анимации, эмодзи и эффекты приостанавливаются. Сообщения и уведомления работают как обычно."),
		.kind = S::Kind::Bool,
		.defaultValue = true,
	});
	lag({
		.key = u"lag.toaster"_q,
		.title = Tr("Режим тостера"),
		.description = Tr("Максимальная экономия всегда: без анимаций и эффектов. Для слабых компьютеров и ноутбуков на батарее."),
		.kind = S::Kind::Bool,
		.defaultValue = false,
	});
	lag({
		.key = u"lag.level"_q,
		.title = Tr("Что отключать при лагах"),
		.description = Tr("Мягко: фон чата, эффекты, спойлеры. Средне: ещё стикеры и эмодзи. Жёстко: всё."),
		.kind = S::Kind::Choice,
		.defaultValue = 1,
		.choices = {
			Tr("Мягко"),
			Tr("Средне"),
			Tr("Жёстко"),
		},
		.dependsOn = u"lag.enabled"_q,
	});
	lag({
		.key = u"lag.sensitivity"_q,
		.title = Tr("Сколько подвисаний за 10 секунд считать лагом"),
		.description = Tr("Меньше число: реагирует раньше."),
		.kind = S::Kind::Int,
		.defaultValue = 8,
		.minValue = 3,
		.maxValue = 40,
		.dependsOn = u"lag.enabled"_q,
	});
	lag({
		.key = u"lag.calm"_q,
		.title = Tr("Через сколько секунд тишины вернуть всё"),
		.kind = S::Kind::Int,
		.defaultValue = 45,
		.minValue = 10,
		.maxValue = 600,
		.dependsOn = u"lag.enabled"_q,
	});
	lag({
		.key = u"lag.cache"_q,
		.title = Tr("Чистить кэш при лагах"),
		.description = Tr("Только если он больше предела ниже. Не чаще раза в полчаса."),
		.kind = S::Kind::Bool,
		.defaultValue = false,
		.dependsOn = u"lag.enabled"_q,
	});
	lag({
		.key = u"lag.cache_mb"_q,
		.title = Tr("Предел кэша, МБ"),
		.kind = S::Kind::Int,
		.defaultValue = 2048,
		.minValue = 256,
		.maxValue = 20480,
		.dependsOn = u"lag.cache"_q,
	});
	lag({
		.key = u"lag.notify"_q,
		.title = Tr("Говорить, когда режим включается"),
		.kind = S::Kind::Bool,
		.defaultValue = true,
		.dependsOn = u"lag.enabled"_q,
	});
	lag({
		.key = u"lag.emoji_cap"_q,
		.title = Tr("Сколько эмодзи играет одновременно"),
		.description = Tr("Остальные премиум-эмодзи замирают на кадре. 0 значит без ограничения."),
		.kind = S::Kind::Int,
		.defaultValue = 12,
		.minValue = 0,
		.maxValue = 200,
	});
	lag({
		.key = u"lag.sticker_cap"_q,
		.title = Tr("Сколько стикеров играет одновременно"),
		.description = Tr("Остальные анимированные стикеры замирают на кадре. 0 значит без ограничения."),
		.kind = S::Kind::Int,
		.defaultValue = 3,
		.minValue = 0,
		.maxValue = 50,
	});
	lag({
		.key = u"lag.fps"_q,
		.title = Tr("Счётчик кадров"),
		.description = Tr("Плавающий оверлей с текущим fps и 1% low за последние 5 секунд. Нужен для замеров."),
		.kind = S::Kind::Bool,
		.defaultValue = false,
	});
	S::Register({
		.key = u"lag.autostart"_q,
		.section = StartSection(),
		.title = Tr("Запускать вместе с системой"),
		.kind = S::Kind::Bool,
		.defaultValue = false,
		.category = u"look"_q,
	});
	S::Register({
		.key = u"lag.background"_q,
		.section = StartSection(),
		.title = Tr("Крестик убирает окно в фон"),
		.description = Tr("Клиент продолжает работать со свёрнутым окном, уведомления не останавливаются. Если выключить, крестик закрывает клиент."),
		.kind = S::Kind::Bool,
		.defaultValue = true,
		.category = u"look"_q,
	});
}

[[nodiscard]] QString PlayerSection() {
	return Tr("Плеер");
}

void RegisterMusic() {
	const auto add = [&](S::Entry entry) {
		entry.section = PlayerSection();
		entry.category = u"look"_q;
		S::Register(std::move(entry));
	};
	add({
		.key = u"player.cover_colors"_q,
		.title = Tr("Цвета плеера из обложки"),
		.description = Tr("Фон полноэкранного плеера под цвет обложки и лёгкий оттенок на панели воспроизведения."),
		.kind = S::Kind::Bool,
		.defaultValue = true,
	});
	add({
		.key = u"player.vinyl"_q,
		.title = Tr("Обложка в виде диска"),
		.description = Tr("В верхней панели плеера обложка круглая, как виниловый диск: крутится при воспроизведении и плавно останавливается на паузе."),
		.kind = S::Kind::Bool,
		.defaultValue = false,
	});
	add({
		.key = u"player.autoplay_next"_q,
		.title = Tr("Автовоспроизведение следующего"),
		.description = Tr("После трека, голосового или кружка сам включается следующий."),
		.kind = S::Kind::Bool,
		.defaultValue = true,
	});
	add({
		.key = u"music.smart_stream"_q,
		.title = Tr("Умный поток музыки"),
		.description = Tr("Подборка «Для вас» играет без конца: очередь сама пополняется под ваш вкус и текущее настроение, без повторов и пропущенного."),
		.kind = S::Kind::Bool,
		.defaultValue = true,
	});
	add({
		.key = u"music.hint_sticker"_q,
		.title = u"music.hint_sticker"_q,
		.kind = S::Kind::String,
		.defaultValue = QString(),
		.hidden = true,
	});
	add({
		.key = u"lyrics.enabled"_q,
		.title = Tr("Текст песни"),
		.description = Tr("Панель со словами справа от чата, текущая строка подсвечивается. Включается кнопкой в панели плеера."),
		.kind = S::Kind::Bool,
		.defaultValue = true,
		.hidden = true,
	});
	add({
		.key = u"lyrics.plasma"_q,
		.title = Tr("Оттенок обложки за текстом"),
		.description = Tr("Лёгкий оттенок обложки на фоне текста песни."),
		.kind = S::Kind::Bool,
		.defaultValue = true,
	});
	add({
		.key = u"lyrics.comments"_q,
		.title = Tr("Комментарии SoundCloud"),
		.description = Tr("Аватарки справа от текущей строки, а без текста песни на полосе прогресса. Наведи, чтобы прочитать; колёсико переключает комментарии в группе."),
		.kind = S::Kind::Bool,
		.defaultValue = true,
	});
	Mokagram::Music::RegisterMotionSettings();
	Mokagram::Music::RegisterSoundCloudSettings();
	// The login row shows the connected account, as on the mokaGram page.
	static auto lifetime = rpl::lifetime();
	Mokagram::Music::Account::Changes(
	) | rpl::on_next([] {
		const auto info = Mokagram::Music::Account::User();
		S::SetString(
			u"music.soundcloud_login"_q,
			!Mokagram::Music::Account::IsLoggedIn()
				? Tr("Подключить")
				: info.username.isEmpty()
				? Tr("Подключено")
				: (u"@"_q + info.username));
	}, lifetime);
}

void IntBox(not_null<Ui::GenericBox*> box, const S::Entry &entry) {
	const auto key = entry.key;
	const auto min = entry.minValue;
	const auto max = entry.maxValue;
	StaticGram::BoxParts::SetupBox(box);
	box->setTitle(rpl::single(entry.title));
	const auto input = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		rpl::single(u"%1 - %2"_q.arg(min).arg(max)),
		QString::number(S::Int(key))));
	input->setMaxLength(QString::number(max).size());
	box->setFocusCallback([=] { input->setFocusFast(); });
	const auto save = [=] {
		auto ok = false;
		const auto value = input->getLastText().toInt(&ok);
		if (!ok || value < min || value > max) {
			input->showError();
			return;
		}
		S::SetInt(key, value);
		box->closeBox();
	};
	input->submits() | rpl::on_next(save, input->lifetime());
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void AddEntry(
		not_null<Window::SessionController*> controller,
		not_null<Ui::VerticalLayout*> card,
		const S::Entry &entry) {
	const auto key = entry.key;
	auto enabled = entry.dependsOn.isEmpty()
		? rpl::single(true)
		: S::BoolValue(entry.dependsOn);
	const auto wrap = card->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			card,
			object_ptr<Ui::VerticalLayout>(card)));
	wrap->toggleOn(std::move(enabled), anim::type::instant);
	const auto inner = wrap->entity();
	const auto &st = st::settingsButtonNoIcon;
	switch (entry.kind) {
	case S::Kind::Bool: {
		const auto button = Settings::AddButtonWithIcon(
			inner,
			rpl::single(entry.title),
			st);
		button->toggleOn(S::BoolValue(key));
		button->toggledChanges(
		) | rpl::filter([=](bool value) {
			return value != S::Bool(key);
		}) | rpl::on_next([=](bool value) {
			S::SetBool(key, value);
		}, button->lifetime());
	} break;
	case S::Kind::Int: {
		const auto copy = entry;
		Settings::AddButtonWithLabel(
			inner,
			rpl::single(entry.title),
			S::IntValue(key) | rpl::map([](int value) {
				return QString::number(value);
			}),
			st
		)->setClickedCallback([=] {
			controller->show(Box(IntBox, copy));
		});
	} break;
	case S::Kind::Choice: {
		const auto choices = entry.choices;
		const auto title = entry.title;
		Settings::AddButtonWithLabel(
			inner,
			rpl::single(entry.title),
			S::IntValue(key) | rpl::map([=](int value) {
				return (value >= 0 && value < choices.size())
					? choices[value]
					: QString();
			}),
			st
		)->setClickedCallback([=] {
			const auto options = std::vector<QString>(
				choices.begin(),
				choices.end());
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				SingleChoiceBox(box, {
					.title = rpl::single(title),
					.options = options,
					.initialSelection = S::Int(key),
					.callback = [=](int index) { S::SetInt(key, index); },
				});
			}));
		});
	} break;
	case S::Kind::String: {
		// A row-button (SoundCloud login): the value is the label.
		if (const auto action = entry.action) {
			Settings::AddButtonWithLabel(
				inner,
				rpl::single(entry.title),
				S::StringValue(key),
				st
			)->setClickedCallback(action);
		}
	} break;
	case S::Kind::Color:
		break;
	}
	if (!entry.description.isEmpty()) {
		inner->add(
			object_ptr<Ui::FlatLabel>(
				inner,
				entry.description,
				st::boxDividerLabel),
			st::defaultBoxDividerLabelPadding);
	}
}

void SectionsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		QString title,
		std::vector<QString> sections) {
	StaticGram::BoxParts::SetupBox(box);
	box->setTitle(rpl::single(title));
	box->setWidth(st::boxWideWidth);
	const auto content = box->verticalLayout();
	for (const auto &section : sections) {
		auto card = (Ui::VerticalLayout*)nullptr;
		auto note = QString();
		for (const auto &entry : S::Entries()) {
			const auto visible = !entry.hidden
				|| (entry.action
					&& entry.key.startsWith(u"music.soundcloud_"_q));
			if (entry.section != section || !visible) {
				continue;
			}
			if (!card) {
				card = StaticGram::BoxParts::AddCard(content);
				if (sections.size() > 1) {
					StaticGram::BoxParts::AddCardTitle(
						card,
						rpl::single(section));
				}
			}
			if (note.isEmpty()) {
				note = entry.sectionNote;
			}
			AddEntry(controller, card, entry);
		}
		if (card && !note.isEmpty()) {
			StaticGram::BoxParts::AddCardNote(content, rpl::single(note));
		}
	}
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace

void RegisterSettings() {
	static auto registered = false;
	if (registered) {
		return;
	}
	registered = true;
	Mokagram::Streamer::RegisterSettings();
	Mokagram::Quote::RegisterSettings();
	RegisterLag();
	RegisterMusic();
}

void ShowStreamerBox(not_null<Window::SessionController*> controller) {
	controller->show(Box(
		SectionsBox,
		controller,
		Tr("Режим стримера"),
		std::vector<QString>{ Tr("Режим стримера") }));
}

void ShowMusicSettingsBox(not_null<Window::SessionController*> controller) {
	controller->show(Box(
		SectionsBox,
		controller,
		Tr("Музыка"),
		std::vector<QString>{ PlayerSection(), QString() }));
}

void ShowPerformanceBox(not_null<Window::SessionController*> controller) {
	controller->show(Box(
		SectionsBox,
		controller,
		Tr("Производительность"),
		std::vector<QString>{ LagSection(), StartSection() }));
}

} // namespace StaticGram::MokaFeatures
