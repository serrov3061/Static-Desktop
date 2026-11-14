// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mokagram-id: 8b14f3d2
#include "mokagram/music/mokagram_soundcloud_login.h"

#include "mokagram/lang/mokagram_lang.h"
#include "mokagram/music/mokagram_soundcloud.h"
#include "mokagram/plugins/mokagram_plugin_install.h"
#include "mokagram/ui/mokagram_box_parts.h"

#include "core/application.h"
#include "lang/lang_keys.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/toast/toast.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "main/main_session.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QClipboard>
#include <QtGui/QDesktopServices>
#include <QtGui/QPainterPath>
#include <QtSvg/QSvgRenderer>
#include <QtGui/QGuiApplication>


namespace Mokagram::Music {
namespace {

constexpr auto kSoundCloudUrl = "https://soundcloud.com/";

// Контур логотипа SoundCloud из simple-icons (CC0), viewBox 24x24.
constexpr auto kSoundCloudPath = "M23.999 14.165c-.052 1.796-1.612 3.169-3.4 3.169h-8.18a.68.68 0 0 1-.675-.683V7.862a.747.747 0 0 1 .452-.724s.75-.513 2.333-.513a5.364 5.364 0 0 1 2.763.755 5.433 5.433 0 0 1 2.57 3.54c.282-.08.574-.121.868-.12.884 0 1.73.358 2.347.992s.948 1.49.922 2.373ZM10.721 8.421c.247 2.98.427 5.697 0 8.672a.264.264 0 0 1-.53 0c-.395-2.946-.22-5.718 0-8.672a.264.264 0 0 1 .53 0ZM9.072 9.448c.285 2.659.37 4.986-.006 7.655a.277.277 0 0 1-.55 0c-.331-2.63-.256-5.02 0-7.655a.277.277 0 0 1 .556 0Zm-1.663-.257c.27 2.726.39 5.171 0 7.904a.266.266 0 0 1-.532 0c-.38-2.69-.257-5.21 0-7.904a.266.266 0 0 1 .532 0Zm-1.647.77a26.108 26.108 0 0 1-.008 7.147.272.272 0 0 1-.542 0 27.955 27.955 0 0 1 0-7.147.275.275 0 0 1 .55 0Zm-1.67 1.769c.421 1.865.228 3.5-.029 5.388a.257.257 0 0 1-.514 0c-.21-1.858-.398-3.549 0-5.389a.272.272 0 0 1 .543 0Zm-1.655-.273c.388 1.897.26 3.508-.01 5.412-.026.28-.514.283-.54 0-.244-1.878-.347-3.54-.01-5.412a.283.283 0 0 1 .56 0Zm-1.668.911c.4 1.268.257 2.292-.026 3.572a.257.257 0 0 1-.514 0c-.241-1.262-.354-2.312-.023-3.572a.283.283 0 0 1 .563 0Z";

[[nodiscard]] QString Tr(const char *text) {
	return QString::fromUtf8(Mokagram::Lang::Tr(text));
}

// oauth_token SoundCloud: JWT, три части через точку, каждая из base64url.
// Иногда встречается и старый вид "2-293746-185937845-AbCd".
[[nodiscard]] QRegularExpression TokenPattern() {
	return QRegularExpression(
		u"^([A-Za-z0-9_-]{10,}\\.[A-Za-z0-9_-]{10,}\\.[A-Za-z0-9_-]{10,}"
		u"|[0-9][0-9]*-[0-9]+-[0-9]+-[A-Za-z0-9_-]{6,})$"_q);
}

// Токен приходит из консоли на странице SoundCloud: сам токен, JSON
// {"oauth_token":"..."} или строка "TOKEN: ..."; редко в base64.
[[nodiscard]] QString TokenFromText(const QString &text) {
	const auto trimmed = text.trimmed();
	const auto bare = trimmed.startsWith(u"TOKEN:"_q)
		? trimmed.mid(6).trimmed()
		: trimmed;
	if (TokenPattern().match(bare).hasMatch() && bare.size() <= 4096) {
		return bare;
	}
	const auto pattern = QRegularExpression(
		u"\"(?:oauth_token|access_token)\"\\s*:\\s*\""
		u"([A-Za-z0-9_.-]{20,4096})\""_q);
	if (const auto found = pattern.match(trimmed); found.hasMatch()) {
		return found.captured(1);
	}
	const auto decoded = QString::fromUtf8(
		QByteArray::fromBase64(trimmed.toLatin1()));
	if (decoded != trimmed) {
		if (const auto found = pattern.match(decoded); found.hasMatch()) {
			return found.captured(1);
		}
	}
	return QString();
}

// StaticGram: the automatic login of mokaGram ran a python3 helper that read
// the SoundCloud cookie out of the local browsers. Not in our client: the
// token is entered by hand (the button opens soundcloud.com).

} // namespace

void PaintSoundCloudBadge(QPainter &p, const QRectF &r) {
	auto hq = PainterHighQualityEnabler(p);
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(0xFF, 0x55, 0x00));
	p.drawRoundedRect(r, r.width() * 0.24, r.height() * 0.24);

	// Фирменный знак SoundCloud (контур из simple-icons, CC0), белым.
	static auto renderer = QSvgRenderer(
		QByteArray("<svg viewBox=\"0 0 24 24\" xmlns=\"http://www.w3.org/2000/svg\">"
			"<path fill=\"#fff\" d=\"")
		+ QByteArray(kSoundCloudPath)
		+ QByteArray("\"/></svg>"));
	const auto size = r.width() * 0.66;
	renderer.render(&p, QRectF(
		r.center().x() - size / 2,
		r.center().y() - size / 2,
		size,
		size));
}

bool OpenSoundCloudInBrowser() {
	return QDesktopServices::openUrl(QUrl(QString::fromLatin1(kSoundCloudUrl)));
}

void ShowSoundCloudLogin(Fn<void()> onLoggedIn, Fn<void()> onClosed) {
	const auto window = Core::App().activeWindow();
	const auto controller = window ? window->sessionController() : nullptr;
	if (!controller) {
		Ui::Toast::Show(Tr("Не удалось открыть окно входа."));
		return;
	}
	auto done = std::make_shared<bool>(false);
	const auto finish = [=](bool success) {
		if (*done) {
			return;
		}
		*done = true;
		if (success && onLoggedIn) {
			onLoggedIn();
		} else if (!success && onClosed) {
			onClosed();
		}
	};

	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		Plugins::RoundedWindow(box);
		const auto s = [](int v) { return style::ConvertScale(v); };
		const auto content = box->verticalLayout();

		// Шапка как в окне «Информация о сообщении»: закрыть и название.
		const auto header = content->add(
			object_ptr<Ui::RpWidget>(content),
			QMargins(s(10), s(10), s(10), 0));
		header->resize(header->width(), s(40));
		const auto close = Plugins::MakeHeaderButton(
			header,
			true,
			[=] { box->closeBox(); });
		const auto title = Ui::CreateChild<Ui::FlatLabel>(
			header,
			Tr("Вход в SoundCloud"),
			st::boxTitle);
		header->widthValue() | rpl::on_next([=](int width) {
			close->moveToLeft(0, 0);
			title->moveToLeft((width - title->width()) / 2, s(6));
		}, header->lifetime());
		Ui::AddSkip(content, s(8));

		// Карточка: значок и одна строка, она же показывает ход входа.
		const auto card = content->add(
			object_ptr<Ui::VerticalLayout>(content),
			QMargins(s(14), s(4), s(14), 0));
		card->paintOn([=](QPainter &p) {
			BoxParts::PaintCard(p, card->rect());
		});
		const auto badge = card->add(
			object_ptr<Ui::RpWidget>(card),
			QMargins(0, s(22), 0, 0));
		badge->resize(badge->width(), s(72));
		badge->paintOn([=](QPainter &p) {
			const auto size = s(72);
			PaintSoundCloudBadge(p, QRectF(
				(badge->width() - size) / 2.,
				0.,
				size,
				size));
		});
		static const auto centered = [] {
			auto result = st::boxLabel;
			result.align = style::al_top;
			return result;
		}();
		const auto status = card->add(
			object_ptr<Ui::FlatLabel>(
				card,
				Tr("Возьмём вход из вашего браузера. Достаточно один раз войти на soundcloud.com."),
				centered),
			QMargins(s(24), s(16), s(24), s(22)));

		// Ручной ввод спрятан: нужен, только если автоматика не сработала.
		const auto manual = content->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				content,
				object_ptr<Ui::VerticalLayout>(content)));
		manual->toggle(false, anim::type::instant);
		const auto manualCard = manual->entity()->add(
			object_ptr<Ui::VerticalLayout>(manual->entity()),
			QMargins(s(14), s(8), s(14), 0));
		manualCard->paintOn([=](QPainter &p) {
			BoxParts::PaintCard(p, manualCard->rect());
		});
		manualCard->add(
			object_ptr<Ui::FlatLabel>(
				manualCard,
				Tr("Токен лежит в cookie oauth_token на soundcloud.com."),
				st::boxDividerLabel),
			QMargins(s(16), s(14), s(16), 0));
		const auto field = manualCard->add(
			object_ptr<Ui::InputField>(
				manualCard,
				BoxParts::CardField(st::defaultInputField),
				rpl::single(Tr("Токен SoundCloud")),
				TokenFromText(QGuiApplication::clipboard()->text())),
			QMargins(0, s(4), 0, s(6)));

		const auto busy = std::make_shared<bool>(false);
		const auto guarded = QPointer<Ui::GenericBox>(box);
		const auto setStatus = [=](const QString &text) {
			status->setText(text);
		};
		const auto login = [=](const QString &token) {
			*busy = true;
			setStatus(Tr("Проверяю вход…"));
			Account::Login(token, [=](QString error) {
				*busy = false;
				if (!guarded) {
					return;
				}
				if (!error.isEmpty()) {
					setStatus(error);
					return;
				}
				guarded->closeBox();
				finish(true);
			});
		};


		const auto submit = [=] {
			if (*busy) {
				return;
			}
			if (manual->toggled()) {
				const auto token = TokenFromText(field->getLastText());
				if (!token.isEmpty()) {
					login(token);
					return;
				} else if (!field->getLastText().trimmed().isEmpty()) {
					field->showError();
					return;
				}
			}
			if (!manual->toggled()) {
				OpenSoundCloudInBrowser();
			}
			setStatus(Tr("Войдите на soundcloud.com и вставьте токен из cookie oauth_token."));
			manual->toggle(true, anim::type::normal);
		};
		field->submits() | rpl::on_next(submit, field->lifetime());

		// Две пилюли на всю ширину, как кнопка «Копировать» в эталоне.
		static const auto stMain = [] {
			auto result = st::defaultActiveButton;
			result.height = style::ConvertScale(44);
			result.textTop = style::ConvertScale(12);
			result.radius = style::ConvertScale(22);
			result.fullRadius = true;
			return result;
		}();
		static const auto stSecond = [] {
			auto result = BoxParts::PillLight();
			result.height = style::ConvertScale(44);
			result.textTop = style::ConvertScale(12);
			result.radius = style::ConvertScale(22);
			return result;
		}();
		const auto addPill = [=](
				const style::RoundButton &st,
				const QString &text,
				Fn<void()> callback,
				QMargins margins) {
			const auto row = content->add(
				object_ptr<Ui::RpWidget>(content),
				margins);
			row->resize(row->width(), st.height);
			const auto button = Ui::CreateChild<Ui::RoundButton>(
				row,
				rpl::single(text),
				st);
			row->widthValue() | rpl::on_next([=](int width) {
				button->setFullWidth(width);
				button->moveToLeft(0, 0);
			}, row->lifetime());
			button->setClickedCallback(std::move(callback));
			return button;
		};
		addPill(
			stMain,
			Tr("Войти через браузер"),
			submit,
			QMargins(s(14), s(12), s(14), 0));
		addPill(
			stSecond,
			Tr("Токен вручную"),
			[=] {
				manual->toggle(!manual->toggled(), anim::type::normal);
				if (manual->toggled()) {
					field->setFocusFast();
				}
			},
			QMargins(s(14), s(8), s(14), s(14)));

		box->boxClosing() | rpl::on_next([=] {
			crl::on_main([=] { finish(false); });
		}, box->lifetime());
	}));
}

void RefreshSoundCloudToken(Fn<void(bool)> done) {
	done(false);
}

} // namespace Mokagram::Music
