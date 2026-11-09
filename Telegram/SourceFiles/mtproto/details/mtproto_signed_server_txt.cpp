/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mtproto/details/mtproto_signed_server_txt.h"

#include <openssl/bio.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace MTP::details {
namespace {

constexpr auto kPrefix = std::string_view("v1:");

[[nodiscard]] int Base64Value(char c) {
	if (c >= 'A' && c <= 'Z') {
		return c - 'A';
	} else if (c >= 'a' && c <= 'z') {
		return c - 'a' + 26;
	} else if (c >= '0' && c <= '9') {
		return c - '0' + 52;
	} else if (c == '+') {
		return 62;
	} else if (c == '/') {
		return 63;
	}
	return -1;
}

// Strict standard base64: padded to a multiple of 4, '=' only at the very
// end. Android's decoder (base64DecodeAlloc) has the same length rule, so
// a record accepted here is accepted there and vice versa for every record
// sg-cfg-txt.py can produce.
[[nodiscard]] std::optional<std::vector<unsigned char>> Base64Decode(
		std::string_view in) {
	if (in.empty() || in.size() % 4 != 0) {
		return std::nullopt;
	}
	auto result = std::vector<unsigned char>();
	result.reserve(in.size() / 4 * 3);
	for (auto i = std::size_t(); i != in.size(); i += 4) {
		const auto last = (i + 4 == in.size());
		const auto pad = (in[i + 3] == '=' ? 1 : 0)
			+ (in[i + 2] == '=' ? 1 : 0);
		if ((pad > 0 && !last) || (in[i + 2] == '=' && in[i + 3] != '=')) {
			return std::nullopt;
		}
		auto chunk = std::uint32_t();
		for (auto j = 0; j != 4; ++j) {
			const auto c = in[i + j];
			const auto value = (j >= 4 - pad) ? 0 : Base64Value(c);
			if (value < 0) {
				return std::nullopt;
			}
			chunk = (chunk << 6) | std::uint32_t(value);
		}
		result.push_back((unsigned char)(chunk >> 16));
		if (pad < 2) {
			result.push_back((unsigned char)(chunk >> 8));
		}
		if (pad < 1) {
			result.push_back((unsigned char)chunk);
		}
	}
	return result;
}

struct BioDeleter {
	void operator()(BIO *value) const {
		BIO_free(value);
	}
};

struct RsaDeleter {
	void operator()(RSA *value) const {
		RSA_free(value);
	}
};

[[nodiscard]] bool VerifyPkcs1Sha256(
		std::string_view publicKeyPem,
		std::string_view message,
		const std::vector<unsigned char> &signature) {
	const auto bio = std::unique_ptr<BIO, BioDeleter>(BIO_new_mem_buf(
		publicKeyPem.data(),
		int(publicKeyPem.size())));
	if (!bio) {
		return false;
	}
	const auto rsa = std::unique_ptr<RSA, RsaDeleter>(
		PEM_read_bio_RSAPublicKey(bio.get(), nullptr, nullptr, nullptr));
	if (!rsa) {
		return false;
	}
	unsigned char digest[SHA256_DIGEST_LENGTH];
	SHA256(
		reinterpret_cast<const unsigned char*>(message.data()),
		message.size(),
		digest);
	return RSA_verify(
		NID_sha256,
		digest,
		SHA256_DIGEST_LENGTH,
		signature.data(),
		(unsigned int)signature.size(),
		rsa.get()) == 1;
}

} // namespace

std::string UnquoteDnsTxt(std::string_view value) {
	const auto spaces = std::string_view(" \t\r\n");
	const auto from = value.find_first_not_of(spaces);
	if (from == std::string_view::npos) {
		return {};
	}
	value = value.substr(from, value.find_last_not_of(spaces) - from + 1);
	if (value.front() != '"') {
		return std::string(value);
	}
	auto result = std::string();
	auto quoted = false;
	for (auto i = std::size_t(); i != value.size(); ++i) {
		const auto c = value[i];
		if (c == '"') {
			quoted = !quoted;
		} else if (c == '\\' && quoted && i + 1 < value.size()) {
			result.push_back(value[++i]);
		} else if (quoted) {
			result.push_back(c);
		}
	}
	return result;
}

std::string VerifySignedServerTxt(
		std::string_view txt,
		std::string_view publicKeyPem,
		int pinnedPort) {
	if (txt.substr(0, kPrefix.size()) != kPrefix) {
		return {};
	}
	const auto ipEnd = txt.find(':', kPrefix.size());
	if (ipEnd == std::string_view::npos) {
		return {};
	}
	const auto portEnd = txt.find(':', ipEnd + 1);
	if (portEnd == std::string_view::npos) {
		return {};
	}
	const auto ip = txt.substr(kPrefix.size(), ipEnd - kPrefix.size());
	const auto port = txt.substr(ipEnd + 1, portEnd - ipEnd - 1);
	if (ip.empty() || port != std::to_string(pinnedPort)) {
		return {};
	}
	const auto signature = Base64Decode(txt.substr(portEnd + 1));
	if (!signature || signature->empty()) {
		return {};
	}
	const auto payload = txt.substr(
		kPrefix.size(),
		portEnd - kPrefix.size());
	if (!VerifyPkcs1Sha256(publicKeyPem, payload, *signature)) {
		return {};
	}
	return std::string(ip);
}

} // namespace MTP::details
