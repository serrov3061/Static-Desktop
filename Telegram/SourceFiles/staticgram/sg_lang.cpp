/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_lang.h"

#include "lang/lang_instance.h"
#include "lang/lang_keys.h"
#include "lang/lang_tag.h" // kTextCommand.
#include "lang_auto_counts.h" // kKeysCount.

#include <QtCore/QHash>
#include <QtCore/QRegularExpression>

namespace StaticGram {
namespace {

struct Phrase {
	const char *key = nullptr;
	const char *value = nullptr;
};

// Russian for the StaticGram keys, used while the cloud language pack does
// not have them (the same texts as the Android values-ru).
constexpr auto kRussian = std::to_array<Phrase>({
	{ "lng_sg_badge_title", "exteraStatic бейдж" },
	{ "lng_sg_badge_preview", "Предпросмотр" },
	{ "lng_sg_badge_emoji", "Эмодзи бейджа" },
	{ "lng_sg_badge_text", "Текст бейджа" },
	{ "lng_sg_badge_text_hint", "Показывается при нажатии на ваш бейдж" },
	{ "lng_sg_badge_custom_info", "У вас кастомный бейдж: выберите любой эмодзи и подпись. Изменения увидят все после подтверждения сервером." },
	{ "lng_sg_badge_standard_info", "У вас стандартный бейдж exteraStatic. Улучшите его до кастомного, чтобы выбрать любой эмодзи и подпись." },
	{ "lng_sg_badge_none_info", "Поддержите exteraStatic и получите бейдж рядом с именем — его видят все." },
	{ "lng_sg_badge_unavailable", "Бейджи временно недоступны." },
	{ "lng_sg_badge_settings", "Настроить бейдж" },
	{ "lng_sg_badge_buy", "Купить бейдж" },
	{ "lng_sg_badge_upgrade", "Улучшить до кастомного" },
	{ "lng_sg_badge_save", "Сохранить бейдж" },
	{ "lng_sg_badge_saved", "Бейдж обновлён." },
	{ "lng_sg_badge_save_error", "Не удалось обновить бейдж: {reason}" },
	{ "lng_sg_badge_unknown_error", "неизвестная ошибка" },
	{ "lng_sg_badge_supporter", "{name} поддержал(а) StaticGram и получил(а) бейдж." },
	{ "lng_sg_badge_developer", "{name} — разработчик StaticGram." },
	{ "lng_sg_badge_sheet_title", "Поддержать StaticGram" },
	{ "lng_sg_badge_sheet_info", "Поддерживая проект, вы получаете уникальный бейдж рядом с именем, который видят все." },
	{ "lng_sg_badge_step1_title", "Выберите бейдж" },
	{ "lng_sg_badge_step1_info", "Бейдж — {supporter}. Кастомный со своим эмодзи и подписью — {custom}. Улучшение — {upgrade}." },
	{ "lng_sg_badge_step2_title", "Оплатите в @StaticGramApp" },
	{ "lng_sg_badge_step2_info", "СБП, Stars или TON — подтверждение присылать не нужно." },
	{ "lng_sg_badge_step3_title", "Получите бейдж" },
	{ "lng_sg_badge_step3_info", "Бейдж выдаётся автоматически и виден всем. Кастомный можно менять в настройках «exteraStatic бейдж»." },
	{ "lng_sg_frame_title", "Рамка аватара" },
	{ "lng_sg_frame_none", "Нет" },
	{ "lng_sg_frame_save", "Сохранить рамку" },
	{ "lng_sg_frame_saved", "Рамка аватара обновлена" },
	{ "lng_sg_frame_not_included", "Эта рамка не входит в ваш тариф." },
	{ "lng_sg_frame_info", "Рамку вокруг вашего аватара видят все пользователи exteraStatic. Пока у вас есть активная история, вместо рамки показывается кольцо истории." },
	{ "lng_sg_frame_style", "Стиль" },
	{ "lng_sg_frame_need_badge", "Рамки аватара входят в бейдж exteraStatic." },
	{ "lng_sg_frame_need_badge_info", "Рамка входит в бейдж: купите бейдж — получите рамку с настройками. Её видят все пользователи exteraStatic." },
	{ "lng_sg_frame_inferno", "Инферно" },
	{ "lng_sg_frame_pixel", "Пиксель-аркада" },
	{ "lng_sg_frame_solitaire", "Солитер" },
	{ "lng_sg_frame_cosmos", "Космос" },
	{ "lng_sg_frame_frost", "Иней" },
	{ "lng_sg_frame_settings", "Настройки" },
	{ "lng_sg_frame_palette", "Палитра" },
	{ "lng_sg_frame_metal", "Металл" },
	{ "lng_sg_frame_gem", "Камень" },
	{ "lng_sg_frame_gempos", "Положение камня" },
	{ "lng_sg_frame_embers", "Искры" },
	{ "lng_sg_frame_stars", "Пиксельные звёзды" },
	{ "lng_sg_frame_planet", "Планета" },
	{ "lng_sg_frame_meteors", "Падающие звёзды" },
	{ "lng_sg_frame_snow", "Снегопад" },
	{ "lng_sg_frame_cap", "Снежная шапка" },
	{ "lng_sg_frame_animation", "Анимация" },
	{ "lng_sg_frame_speed", "Скорость анимации" },
	{ "lng_sg_frame_glow", "Свечение" },
	{ "lng_sg_frame_opt_fire", "Огонь" },
	{ "lng_sg_frame_opt_blue", "Синий" },
	{ "lng_sg_frame_opt_green", "Зелёный" },
	{ "lng_sg_frame_opt_purple", "Фиолетовый" },
	{ "lng_sg_frame_opt_pink", "Розовый" },
	{ "lng_sg_frame_opt_rainbow", "Радуга" },
	{ "lng_sg_frame_opt_pastel", "Пастель" },
	{ "lng_sg_frame_opt_neon", "Неон" },
	{ "lng_sg_frame_opt_mono", "Моно" },
	{ "lng_sg_frame_opt_gold", "Золото" },
	{ "lng_sg_frame_opt_platinum", "Платина" },
	{ "lng_sg_frame_opt_rose", "Розовое золото" },
	{ "lng_sg_frame_opt_black", "Чёрный" },
	{ "lng_sg_frame_opt_diamond", "Бриллиант" },
	{ "lng_sg_frame_opt_ruby", "Рубин" },
	{ "lng_sg_frame_opt_emerald", "Изумруд" },
	{ "lng_sg_frame_opt_sapphire", "Сапфир" },
	{ "lng_sg_frame_opt_amethyst", "Аметист" },
	{ "lng_sg_frame_opt_none", "Нет" },
	{ "lng_sg_frame_opt_bottom", "Снизу" },
	{ "lng_sg_frame_opt_top", "Сверху" },
	{ "lng_sg_frame_opt_right", "Справа" },
	{ "lng_sg_frame_opt_violet", "Фиолетовый" },
	{ "lng_sg_frame_opt_red", "Красный" },
	{ "lng_sg_frame_opt_teal", "Бирюзовый" },
	{ "lng_sg_frame_opt_ice", "Лёд" },
	{ "lng_sg_frame_opt_snow", "Снег" },
	{ "lng_sg_frame_opt_aurora", "Сияние" },
	{ "lng_sg_frame_neko", "Неко" },
	{ "lng_sg_frame_angel", "Ангел" },
	{ "lng_sg_frame_devil", "Демон" },
	{ "lng_sg_frame_whiskers", "Усы" },
	{ "lng_sg_frame_paw", "Лапка" },
	{ "lng_sg_frame_wings", "Крылья" },
	{ "lng_sg_frame_sparkle", "Искорка" },
	{ "lng_sg_frame_tail", "Хвост" },
	{ "lng_sg_frame_pulse", "Пульсация огня" },
	{ "lng_sg_frame_opt_white", "Белый" },
	{ "lng_sg_frame_opt_ginger", "Рыжий" },
	{ "lng_sg_frame_opt_lilac", "Сиреневый" },
	{ "lng_sg_frame_opt_silver", "Серебро" },
	{ "lng_sg_frame_opt_sky", "Небо" },
	{ "lng_sg_banner_title", "Баннер профиля" },
	{ "lng_sg_banner_choose", "Выбрать баннер" },
	{ "lng_sg_banner_remove", "Удалить баннер" },
	{ "lng_sg_banner_info", "Картинка или короткое видео до 15 секунд за шапкой вашего профиля. Доступно с бейджем exteraStatic. Баннер видят все пользователи exteraStatic." },
	{ "lng_sg_banner_uploading", "Загрузка баннера..." },
	{ "lng_sg_banner_saved", "Баннер обновлён" },
	{ "lng_sg_banner_removed", "Баннер удалён" },
	{ "lng_sg_banner_too_large", "Подойдёт картинка или видео MP4 до 15 секунд и 10 МБ." },
	{ "lng_sg_banner_need_badge", "Баннер доступен с бейджем exteraStatic." },
	{ "lng_sg_banner_failed", "Баннер не изменён: {reason}" },
	{ "lng_sg_banner_current", "Текущий баннер" },
	{ "lng_sg_banner_none", "Нет баннера" },
	{ "lng_sg_money_title", "Деньги" },
	{ "lng_sg_money_transactions", "Транзакции" },
	{ "lng_sg_money_collectibles", "Коллекции" },
	{ "lng_sg_money_no_transactions", "Транзакций пока нет" },
	{ "lng_sg_money_no_collectibles", "Коллекций пока нет" },
	{ "lng_sg_money_loading", "Загрузка…" },
	{ "lng_sg_money_load_failed", "Не удалось загрузить. Попробуйте позже." },
	{ "lng_sg_money_send_gram", "Отправить GRAM" },
	{ "lng_sg_money_send_stars", "Отправить Stars" },
	{ "lng_sg_money_edit_card", "Изменить карточку" },
	{ "lng_sg_money_card_gram", "Карта GRAM" },
	{ "lng_sg_money_card_stars", "Карта Stars" },
	{ "lng_sg_money_unit_gram", "GRAM" },
	{ "lng_sg_money_unit_stars", "STARS" },
	{ "lng_sg_money_choose_recipient", "Выберите получателя" },
	{ "lng_sg_money_transfer_title", "Перевод" },
	{ "lng_sg_money_send_to", "Получатель: {user}" },
	{ "lng_sg_money_amount_gram", "Сумма в GRAM" },
	{ "lng_sg_money_amount_stars", "Сумма в Stars" },
	{ "lng_sg_money_comment", "Комментарий (необязательно)" },
	{ "lng_sg_money_balance_gram", "Баланс: {amount} GRAM" },
	{ "lng_sg_money_balance_stars", "Баланс: {amount} Stars" },
	{ "lng_sg_money_enter_amount", "Введите сумму" },
	{ "lng_sg_money_send_gram_amount", "Отправить {amount} GRAM" },
	{ "lng_sg_money_send_stars_amount", "Отправить {amount} Stars" },
	{ "lng_sg_money_sent", "Отправлено {amount} пользователю {user}" },
	{ "lng_sg_money_fee", "Комиссия: {amount} GRAM" },
	{ "lng_sg_money_help", "GRAM и Stars приходят мгновенно. Перевод и ваш комментарий появятся в чате с получателем." },
	{ "lng_sg_money_not_self", "Нельзя отправить деньги самому себе." },
	{ "lng_sg_money_owns", "{user} — на счету {amount}." },
	{ "lng_sg_money_owns_no_gram", "{user} — пока нет GRAM." },
	{ "lng_sg_money_owns_no_stars", "{user} — пока нет Stars." },
	{ "lng_sg_money_public_note", "Ваши карточки и их балансы видны всем, кто открывает ваш профиль." },
	{ "lng_sg_money_user_not_found", "Не удалось найти этого пользователя." },
	{ "lng_sg_money_error_unknown", "Что-то пошло не так. Попробуйте ещё раз." },
	{ "lng_sg_money_error_balance", "Недостаточно средств на балансе." },
	{ "lng_sg_money_error_recipient", "Этому пользователю нельзя отправить деньги." },
	{ "lng_sg_money_error_bot", "Ботам нельзя отправлять деньги." },
	{ "lng_sg_money_error_stars_amount", "Введите от 1 до {max} Stars." },
	{ "lng_sg_money_error_comment", "Комментарий слишком длинный (до {max} символов)." },
	{ "lng_sg_money_error_amount", "Неверная сумма." },
	{ "lng_sg_money_error_code", "Ошибка: {code}" },
	// a2-ui
	{ "lng_sg_stars_amount#one", "{count} Stars" },
	{ "lng_sg_stars_amount#few", "{count} Stars" },
	{ "lng_sg_stars_amount#many", "{count} Stars" },
	{ "lng_sg_stars_amount#other", "{count} Stars" },
	{ "lng_sg_stars_ticker#one", "Stars" },
	{ "lng_sg_stars_ticker#few", "Stars" },
	{ "lng_sg_stars_ticker#many", "Stars" },
	{ "lng_sg_stars_ticker#other", "Stars" },
	{ "lng_sg_transfer_title_gram", "Перевод GRAM" },
	{ "lng_sg_transfer_title_stars", "Перевод Stars" },
	{ "lng_sg_transfer_title_grant", "Начисление" },
	{ "lng_sg_transfer_title_prize", "Приз розыгрыша" },
	{ "lng_sg_transfer_from", "Отправитель" },
	{ "lng_sg_transfer_to", "Получатель" },
	{ "lng_sg_transfer_date", "Дата" },
	{ "lng_sg_transfer_comment", "Комментарий" },
	{ "lng_sg_transfer_address", "Адрес" },
	{ "lng_sg_transfer_id", "ID транзакции" },
	{ "lng_sg_transfer_copied", "Скопировано в буфер обмена." },
	{ "lng_action_gram_transfer_sent", "Вы отправили {user} {amount}" },
	{ "lng_action_gram_transfer_received", "{user} отправил(а) вам {amount}" },
	{ "lng_action_gram_transfer_sent_unknown", "Вы отправили {amount}" },
	{ "lng_action_gram_transfer_received_unknown", "Вы получили {amount}" },
	{ "lng_action_gram_transfer_received_tag", "получено" },
	{ "lng_action_gram_transfer_sent_tag", "отправлено" },
	{ "lng_action_gram_transfer_failed_tag", "ошибка" },
	{ "lng_action_gram_transfer_encrypted_comment", "Зашифрованный комментарий" },
	// p-money2
	{ "lng_sg_money_to_address", "На адрес" },
	{ "lng_sg_money_to_address_about", "Адрес кошелька, имя.ton или @username" },
	{ "lng_sg_money_address_to", "Кому" },
	{ "lng_sg_money_address_hint", "Адрес, имя.ton или @username" },
	{ "lng_sg_money_address_explain", "Вставьте адрес кошелька или введите имя.ton или @username" },
	{ "lng_sg_money_address_valid", "Адрес кошелька" },
	{ "lng_sg_money_address_domain", "Домен имя.ton" },
	{ "lng_sg_money_address_username", "Юзернейм StaticGram" },
	{ "lng_sg_money_external_unsupported", "Перевод на внешние адреса пока недоступен" },
	{ "lng_sg_nft_collectible", "Коллекционный предмет" },
	{ "lng_sg_nft_open", "Открыть" },
	{ "lng_sg_nft_transfer", "Передать" },
	{ "lng_sg_nft_transfer_title", "Передать NFT" },
	{ "lng_sg_nft_transfer_to_user", "Передать пользователю" },
	{ "lng_sg_nft_transfer_to_user_text", "Передать {name} пользователю {user}? NFT попадёт в его кошелёк StaticGram." },
	{ "lng_sg_nft_transfer_to_address", "Отправить на адрес" },
	{ "lng_sg_nft_transfer_to_address_text", "Отправить {name} на этот адрес? Если адрес вне StaticGram, NFT покинет StaticGram." },
	{ "lng_sg_nft_address_hint", "Адрес кошелька" },
	{ "lng_sg_nft_send", "Отправить" },
	{ "lng_sg_nft_sent", "NFT отправлен" },
	{ "lng_sg_nft_only_users", "NFT можно передать только другому пользователю" },
	{ "lng_sg_nft_invalid_address", "Неверный адрес кошелька" },
	{ "lng_sg_nft_domain_unsupported", "Вставьте адрес кошелька: имя.ton для NFT пока не поддерживается" },
	{ "lng_sg_nft_return_to_profile", "Вернуть в профиль" },
	{ "lng_sg_nft_return_to_profile_text", "Вернуть подарок из кошелька в подарки профиля?" },
	{ "lng_sg_nft_returned_to_profile", "Подарок возвращается в профиль" },
	{ "lng_sg_nft_move", "Перенести" },
	{ "lng_sg_nft_move_to_wallet", "Перенести в кошелёк" },
	{ "lng_sg_nft_move_to_wallet_text", "Перенести подарок в кошелёк StaticGram? Он станет NFT в блокчейне StaticGram." },
	{ "lng_sg_nft_moved_to_wallet", "Подарок перенесён в кошелёк" },
	{ "lng_sg_nft_keep_on_profile", "Оставить на странице профиля" },
	{ "lng_sg_nft_copy_address", "Скопировать адрес" },
	{ "lng_sg_nft_address_copied", "Адрес скопирован" },
	{ "lng_sg_nft_error_key_conflict", "Этот запрос уже использован, попробуйте ещё раз" },
	{ "lng_sg_nft_error_external", "Юзернеймы и номера нельзя отправить на внешний адрес" },
	{ "lng_sg_nft_error_withdraw_unavailable", "Этот подарок сейчас нельзя перенести в кошелёк" },
	{ "lng_sg_nft_error_too_early", "Этот подарок пока нельзя перенести в кошелёк" },
	{ "lng_sg_nft_error_busy", "NFT сейчас переносится, попробуйте чуть позже" },
	{ "lng_sg_nft_error_listed", "NFT выставлен на продажу, на аукцион или сдан в аренду" },
	{ "lng_sg_nft_error_invalid", "Этого NFT больше нет в кошельке" },
	{ "lng_sg_nft_error_wallet", "Кошелёк сейчас недоступен" },
	{ "lng_sg_nft_error_disallowed", "Пользователь не принимает коллекционные подарки" },
	{ "lng_sg_card_edit_gram", "Карта GRAM" },
	{ "lng_sg_card_edit_stars", "Карта Stars" },
	{ "lng_sg_card_editor_hint", "Перетаскивайте стикеры, угловой ручкой или колесом мыши меняйте размер (Ctrl+колесо поворачивает). Чтобы убрать стикер, утащите его за край карты. До {max} стикеров." },
	{ "lng_sg_card_sticker_to_front", "Наверх" },
	{ "lng_sg_card_sticker_remove", "Убрать" },
	{ "lng_sg_card_tab_background", "Фон" },
	{ "lng_sg_card_tab_gifts", "Подарки" },
	{ "lng_sg_card_tab_stickers", "Стикеры" },
	{ "lng_sg_card_background_none", "Стандартный фон" },
	{ "lng_sg_card_no_gifts", "У вас пока нет уникальных подарков." },
	{ "lng_sg_card_no_stickers", "Нет статичных стикеров. Добавьте наборы стикеров, чтобы ставить их на карту." },
	{ "lng_sg_card_save", "Сохранить" },
	{ "lng_sg_card_saved", "Карточка сохранена" },
	{ "lng_sg_card_error_not_purchased", "Карточки ещё не куплены. Купить можно в @StaticGramApp." },
	{ "lng_sg_card_error_too_many", "На карте помещается не больше {max} стикеров." },
	{ "lng_sg_card_error_animated", "Анимированные и видеостикеры нельзя ставить на карту." },
	{ "lng_sg_card_error_gift", "Один из подарков вам больше не принадлежит." },
	{ "lng_sg_cards_promo_title", "Карточки StaticGram" },
	{ "lng_sg_cards_promo_text", "Показывайте баланс GRAM и Stars в профиле в виде карточек. Окрасьте их любым своим уникальным подарком и украсьте подарками и стикерами." },
	{ "lng_sg_cards_promo_button", "Купить в @StaticGramApp" },
	// p-misc: .ton domains, support badge number / transfer, official proxies.
	{ "lng_sg_dns_title", "Домены .ton" },
	{ "lng_sg_dns_intro", "Короткое имя вместо адреса кошелька. Оплата один раз в GRAM, домен ваш навсегда." },
	{ "lng_sg_dns_search_hint", "имя" },
	{ "lng_sg_dns_search_empty", "Латинские буквы, цифры и _ , до 32 символов" },
	{ "lng_sg_dns_checking", "Проверяем…" },
	{ "lng_sg_dns_available", "Свободен" },
	{ "lng_sg_dns_buy_for", "Купить за {amount}" },
	{ "lng_sg_dns_state_taken", "Занят" },
	{ "lng_sg_dns_state_yours", "Уже ваш" },
	{ "lng_sg_dns_state_invalid", "Недопустимое имя" },
	{ "lng_sg_dns_on_sale", "Продаётся" },
	{ "lng_sg_dns_my", "Мои домены" },
	{ "lng_sg_dns_my_empty", "У вас пока нет доменов" },
	{ "lng_sg_dns_my_empty_hint", "Найдите свободное имя выше и купите его за GRAM" },
	{ "lng_sg_dns_my_error", "Не удалось загрузить домены" },
	{ "lng_sg_dns_loading", "Загрузка…" },
	{ "lng_sg_dns_buy_text", "NFT домена попадёт в ваш кошелёк StaticGram. Оплата с баланса GRAM." },
	{ "lng_sg_dns_buy_listed_text", "Домен перейдёт в ваш кошелёк StaticGram. Оплата с баланса GRAM." },
	{ "lng_sg_dns_price", "Цена" },
	{ "lng_sg_dns_balance", "Ваш баланс" },
	{ "lng_sg_dns_term", "Срок" },
	{ "lng_sg_dns_term_forever", "Навсегда" },
	{ "lng_sg_dns_seller", "Продавец" },
	{ "lng_sg_dns_low_balance", "Недостаточно GRAM" },
	{ "lng_sg_dns_minting", "Минтится…" },
	{ "lng_sg_dns_minting_text", "Домен оплачен. Минт в блокчейне займёт немного времени, окно можно закрыть." },
	{ "lng_sg_dns_bought", "Готово!" },
	{ "lng_sg_dns_bought_text", "{name} теперь ваш. Сделайте его основным, чтобы он показывался вместо адреса." },
	{ "lng_sg_dns_bought_listed", "Домен куплен, он появится в кошельке через несколько секунд" },
	{ "lng_sg_dns_done", "Готово" },
	{ "lng_sg_dns_set_primary", "Сделать основным" },
	{ "lng_sg_dns_unset_primary", "Убрать основной" },
	{ "lng_sg_dns_primary_set", "{name} теперь ваш основной домен" },
	{ "lng_sg_dns_primary_cleared", "Основной домен убран" },
	{ "lng_sg_dns_primary_chip", "Основной" },
	{ "lng_sg_dns_status", "Статус" },
	{ "lng_sg_dns_status_ready", "Готов" },
	{ "lng_sg_dns_status_minting", "Минтится…" },
	{ "lng_sg_dns_purchased", "Куплен" },
	{ "lng_sg_dns_address", "Адрес" },
	{ "lng_sg_dns_address_copied", "Адрес скопирован." },
	{ "lng_sg_dns_wallet_record", "Адрес кошелька" },
	{ "lng_sg_dns_wallet_record_text", "Куда приходят GRAM, отправленные на {name}." },
	{ "lng_sg_dns_wallet_own", "Ваш кошелёк" },
	{ "lng_sg_dns_wallet_reset", "Мой кошелёк" },
	{ "lng_sg_dns_wallet_saved", "Адрес кошелька сохранён" },
	{ "lng_sg_dns_wallet_hint", "Адрес кошелька" },
	{ "lng_sg_dns_transfer_minting", "Домен ещё минтится" },
	{ "lng_sg_dns_domain_ready", "Домен {name} готов" },
	{ "lng_sg_dns_sell", "Продать" },
	{ "lng_sg_dns_sale_title", "Продать {name}" },
	{ "lng_sg_dns_sale_price_hint", "Цена" },
	{ "lng_sg_dns_sale_you_get", "Вы получите {amount} GRAM (комиссия {percent}%)" },
	{ "lng_sg_dns_sale_fee", "Комиссия {percent}% от цены" },
	{ "lng_sg_dns_sale_bounds", "От {min} до {max} GRAM" },
	{ "lng_sg_dns_sale_listed_for", "Продаётся за {amount} GRAM" },
	{ "lng_sg_dns_sale_list", "Выставить" },
	{ "lng_sg_dns_sale_change", "Изменить цену" },
	{ "lng_sg_dns_sale_cancel", "Снять с продажи" },
	{ "lng_sg_dns_sale_listed", "{name} выставлен на продажу" },
	{ "lng_sg_dns_sale_cancelled", "Снят с продажи" },
	{ "lng_sg_dns_price_changed_new", "Цена изменилась: {amount} GRAM" },
	{ "lng_sg_dns_for_sale", "Продаются" },
	{ "lng_sg_dns_for_sale_empty", "Пока ничего не продаётся" },
	{ "lng_sg_dns_for_sale_more", "Показать ещё" },
	{ "lng_sg_dns_error_unknown", "Что-то пошло не так, попробуйте ещё раз" },
	{ "lng_sg_dns_error_invalid", "Допустимы 1–32 латинские буквы, цифры или _" },
	{ "lng_sg_dns_error_taken", "Этот домен занят" },
	{ "lng_sg_dns_error_not_found", "Домен не найден" },
	{ "lng_sg_dns_error_not_owner", "Этот домен вам не принадлежит" },
	{ "lng_sg_dns_error_price_changed", "Цена изменилась, проверьте и попробуйте ещё раз" },
	{ "lng_sg_dns_error_unavailable", "Домены пока недоступны" },
	{ "lng_sg_dns_error_address", "Неверный адрес кошелька" },
	{ "lng_sg_dns_error_key_conflict", "Этот запрос уже использован, попробуйте ещё раз" },
	{ "lng_sg_dns_error_balance", "Недостаточно средств на балансе." },
	{ "lng_sg_dns_error_not_listed", "Домен не продаётся" },
	{ "lng_sg_dns_error_self_buy", "Это ваш собственный лот" },
	{ "lng_sg_dns_error_price_invalid", "Цена вне допустимого диапазона" },
	{ "lng_sg_dns_error_not_minted", "Домен ещё не заминчен" },
	{ "lng_sg_dns_error_sale_unavailable", "Этот домен сейчас нельзя продать" },
	{ "lng_sg_dns_error_busy", "Домен сейчас передаётся, попробуйте позже" },
	{ "lng_sg_badge_number", "Значок поддержки №{number}" },
	{ "lng_sg_badge_transfer", "Передать другому аккаунту" },
	{ "lng_sg_badge_transfer_title", "Передача значка" },
	{ "lng_sg_badge_transfer_confirm", "Передать значок **{number}** аккаунту **{name}**? Этот аккаунт его потеряет, а следующая передача будет доступна через 30 дней." },
	{ "lng_sg_badge_transfer_other", "Другой аккаунт…" },
	{ "lng_sg_badge_transfer_do", "Передать" },
	{ "lng_sg_badge_transfer_done", "Значок передан аккаунту **{name}**" },
	{ "lng_sg_badge_transfer_cooldown", "Следующая передача будет доступна **{date}**" },
	{ "lng_sg_badge_transfer_failed", "Не удалось передать значок. Попробуйте позже." },
	{ "lng_sg_badge_transfer_has_badge", "У этого аккаунта уже есть значок" },
	{ "lng_sg_badge_peer_title", "**{name}** поддерживает StaticGram" },
	{ "lng_sg_badge_peer_subtitle", "Поддержавшие получают:" },
	{ "lng_sg_badge_perk_badge", "Номерной значок" },
	{ "lng_sg_badge_perk_badge_info", "Значок с вашим номером рядом с именем, его видят все в StaticGram." },
	{ "lng_sg_badge_perk_builds", "Ранние сборки" },
	{ "lng_sg_badge_perk_builds_info", "Закрытый канал с тестовыми версиями до их выхода." },
	{ "lng_sg_badge_perk_regdate", "Даты регистрации" },
	{ "lng_sg_badge_perk_regdate_info", "Более точные даты создания аккаунтов в профилях." },
	{ "lng_sg_badge_how_to_get", "Как получить" },
	{ "lng_sg_badge_got_it", "Понятно" },
	{ "lng_sg_proxy_official_header", "Официальные прокси StaticGram" },
	{ "lng_sg_proxy_official_info", "Работают с сетью StaticGram. Прокси из Telegram с нашим сервером не работают." },
	{ "lng_sg_proxy_official", "Официальный" },
	{ "lng_sg_proxy_not_for_sg", "Этот прокси сделан для Telegram и, скорее всего, не будет работать со StaticGram. Используйте официальные прокси StaticGram или SOCKS5." },
	// p-misc end
});

// The "{tag}" placeholders become the lang tag commands, the way the lang
// pack values are stored, so tr::lng_*(lt_tag, ...) replaces them.
[[nodiscard]] QString PrepareTags(QString value) {
	auto from = 0;
	while (true) {
		const auto open = value.indexOf(QChar('{'), from);
		if (open < 0) {
			break;
		}
		const auto close = value.indexOf(QChar('}'), open + 1);
		if (close < 0) {
			break;
		}
		const auto tag = value.mid(open + 1, close - open - 1).toLatin1();
		const auto index = Lang::GetTagIndex(QLatin1String(tag));
		if (index >= Lang::kTagsCount) {
			from = close + 1;
			continue;
		}
		auto command = QString(4, QChar(Lang::kTextCommand));
		command[1] = QChar(Lang::kTextCommandLangTag);
		command[2] = QChar(0x0020 + index);
		value.replace(open, close - open + 1, command);
		from = open + command.size();
	}
	return value;
}

[[nodiscard]] const QHash<ushort, QString> &Russian() {
	static const auto result = [] {
		auto map = QHash<ushort, QString>();
		for (const auto &phrase : kRussian) {
			const auto index = Lang::GetKeyIndex(
				QLatin1String(phrase.key));
			if (index < Lang::kKeysCount) {
				map.insert(
					index,
					PrepareTags(QString::fromUtf8(phrase.value)));
			}
		}
		return map;
	}();
	return result;
}

[[nodiscard]] bool RussianLanguage() {
	const auto &instance = Lang::GetInstance();
	return instance.id().startsWith(u"ru"_q, Qt::CaseInsensitive)
		|| instance.baseId().startsWith(u"ru"_q, Qt::CaseInsensitive);
}

// Android parity: the interface never says "Telegram" or "TON".
// Links (telegram.org, t.me) are lowercase and stay untouched.
[[nodiscard]] QString Rebrand(QString value) {
	if (value.contains(u"Telegram"_q)) {
		value.replace(u"Telegram"_q, u"StaticGram"_q);
	}
	if (value.contains(u"TON"_q)) {
		static const auto ton = QRegularExpression(u"\\bTON\\b"_q);
		value.replace(ton, u"GRAM"_q);
	}
	return value;
}

} // namespace

QString LangValue(ushort key, QString value) {
	const auto &map = Russian();
	const auto i = map.constFind(key);
	if (i == map.cend()
		|| !RussianLanguage()
		|| value != Lang::GetOriginalValue(key)) {
		return Rebrand(std::move(value));
	}
	return i.value();
}

} // namespace StaticGram
