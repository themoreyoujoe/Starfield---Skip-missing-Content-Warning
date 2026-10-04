// MojoSkipMissingContent - SFSE plugin
//
// Before a save loads, the load menu (0x141EE5304) calls the pre-load check
// 0x141809870(saveInfo, result, skipMask) with skipMask = 0. The check sets result flags:
//   1 / 8 = unsupported save version (left alone)
//   2     = vanilla save loaded with mods ($LoadVanillaSaveWithMods, achievements prompt)
//   4     = save relies on missing plugins / Creations ("Creations missing" flow)
// Any flag routes the load through 0x141849DC0, which shows the prompts. The check itself
// skips the 2 and 4 tests when the same bits are set in skipMask, and then the menu loads
// the save directly, exactly like a save with nothing missing.
//
// This plugin rewrites that one call so the configured bits are always passed.
//
// Settings: Data\SFSE\Plugins\MojoSkipMissingContent.ini (all optional).
//
// VERSION GUARD: addresses are from 1.16.244. Nothing is installed on other versions,
// or when the call site or the check no longer has the expected bytes.

#include <SFSE/SFSE.h>
#include <RE/Starfield.h>

#include <cstring>
#include <fstream>

namespace
{
	constexpr REL::Version  kVerifiedVersion(1, 16, 244, 0);
	constexpr std::uintptr_t kImageBase = 0x140000000;

	constexpr std::uintptr_t kPreLoadCheck = 0x141809870;
	constexpr std::uintptr_t kCallSite = 0x141EE5304;
	constexpr std::array<std::uint8_t, 8> kPreLoadCheckPrologue{ 0x40, 0x53, 0x55, 0x57, 0x48, 0x83, 0xEC, 0x20 };

	constexpr std::uint64_t kSkipVanillaWithMods = 2;
	constexpr std::uint64_t kSkipMissingContent = 4;

	using PreLoadCheck_t = bool (*)(void*, std::uint32_t*, std::uint64_t);

	REL::Offset Off(std::uintptr_t a_abs) { return REL::Offset(a_abs - kImageBase); }

	PreLoadCheck_t g_original = nullptr;

	struct Settings
	{
		bool skipMissingContent = true;
		bool skipVanillaWithMods = true;
	} g_settings;

	void LoadSettings()
	{
		std::ifstream in("Data\\SFSE\\Plugins\\MojoSkipMissingContent.ini");
		if (!in) {
			REX::INFO("No MojoSkipMissingContent.ini; using defaults");
			return;
		}
		auto trim = [](std::string s) {
			const auto b = s.find_first_not_of(" \t\r");
			const auto e = s.find_last_not_of(" \t\r");
			return b == std::string::npos ? std::string{} : s.substr(b, e - b + 1);
		};
		std::string line;
		while (std::getline(in, line)) {
			const auto semi = line.find(';');
			if (semi != std::string::npos) {
				line.resize(semi);
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			const auto key = trim(line.substr(0, eq));
			const auto val = trim(line.substr(eq + 1));
			const bool on = val == "1" || val == "true";
			if (key == "bSkipMissingContent") g_settings.skipMissingContent = on;
			else if (key == "bSkipVanillaSaveWithMods") g_settings.skipVanillaWithMods = on;
		}
	}

	std::uint64_t SkipMask()
	{
		return (g_settings.skipMissingContent ? kSkipMissingContent : 0) |
		       (g_settings.skipVanillaWithMods ? kSkipVanillaWithMods : 0);
	}

	bool PreLoadCheck_Hook(void* a_saveInfo, std::uint32_t* a_result, std::uint64_t a_skip)
	{
		const bool ok = g_original(a_saveInfo, a_result, a_skip | SkipMask());
		if (!ok) {
			REX::INFO("Pre-load check still failed with flags 0x{:X}", *a_result);
		}
		return ok;
	}

	bool InstallHook()
	{
		const auto check = reinterpret_cast<const std::uint8_t*>(Off(kPreLoadCheck).address());
		if (std::memcmp(check, kPreLoadCheckPrologue.data(), kPreLoadCheckPrologue.size()) != 0) {
			REX::ERROR("Pre-load check at {:X} does not have the expected bytes", kPreLoadCheck);
			return false;
		}
		const auto site = reinterpret_cast<const std::uint8_t*>(Off(kCallSite).address());
		std::int32_t rel;
		std::memcpy(&rel, site + 1, sizeof(rel));
		if (site[0] != 0xE8 || Off(kCallSite).address() + 5 + rel != Off(kPreLoadCheck).address()) {
			REX::ERROR("Call site {:X} does not call the pre-load check", kCallSite);
			return false;
		}

		g_original = reinterpret_cast<PreLoadCheck_t>(Off(kPreLoadCheck).address());
		REL::Relocation<std::uintptr_t> target{ Off(kCallSite) };
		target.write_call<5>(PreLoadCheck_Hook);
		REX::INFO("Installed pre-load check hook (skip missing content {}, skip vanilla-save-with-mods {})",
			g_settings.skipMissingContent, g_settings.skipVanillaWithMods);
		return true;
	}
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
	SFSE::Init(a_sfse, { .trampoline = true, .trampolineSize = 32 });

	const auto runtime = a_sfse->RuntimeVersion();
	if (runtime != kVerifiedVersion) {
		REX::WARN("Starfield {} is not verified (built for {}); hook disabled",
			runtime.string(), kVerifiedVersion.string());
		return true;
	}

	LoadSettings();
	if (SkipMask() != 0 && !InstallHook()) {
		REX::ERROR("Game code differs from 1.16.244; hook NOT installed");
	}
	return true;
}
