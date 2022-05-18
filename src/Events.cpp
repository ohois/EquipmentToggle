#include "Events.h"
#include "Graphics.h"
#include "Settings.h"

namespace Events
{
	struct detail
	{
		static bool is_cell_home(const RE::TESObjectCELL* a_cell)
		{
			if (a_cell->IsInteriorCell()) {
				if (const auto loc = a_cell->GetLocation(); loc) {
					return loc->HasKeywordString(PlayerHome) || loc->HasKeywordString(Inn);
				}
			}
			return false;
		}

		static bool process_key(const Key a_key, SlotKeyVec& a_armor, SlotKeyVec& a_weapon, std::function<void(SlotData& a_slots)> a_func)
		{
			auto result = false;

			for (auto& [key, data] : a_armor) {
				if (key == a_key) {
					result = true;
					a_func(data);
					break;
				}
			}

			for (auto& [key, data] : a_weapon) {
				if (key == a_key) {
					result = true;
					a_func(data);
					break;
				}
			}

			return result;
		}

		static bool is_in_menu()
		{
			const auto UI = RE::UI::GetSingleton();
			return UI && (UI->GameIsPaused() || UI->IsModalMenuOpen() || UI->IsApplicationMenuOpen());
		}
	};

	bool Manager::PlayerCombat::thunk(RE::PlayerCharacter* a_this)
	{
		const auto result = func(a_this);
		if (result && !GetSingleton()->playerInCombat) {
			GetSingleton()->playerInCombat = true;
			Graphics::ToggleActorEquipment(a_this, true);
		} else {
			GetSingleton()->playerInCombat = false;
			Graphics::ToggleActorEquipment(a_this, false);
		}
		return result;
	}

    EventResult Manager::ProcessEvent(const RE::TESCombatEvent* evn, RE::BSTEventSource<RE::TESCombatEvent>*)
	{
		if (!evn || !evn->actor) {
			return EventResult::kContinue;
		}

		const auto actor = evn->actor->As<RE::Actor>();
		if (!actor || !Settings::GetSingleton()->CanToggleEquipment(actor)) {
			return EventResult::kContinue;
		}

		switch (*evn->newState) {
		case RE::ACTOR_COMBAT_STATE::kCombat:
			Graphics::ToggleActorEquipment(actor, false);
			break;
		case RE::ACTOR_COMBAT_STATE::kNone:
			Graphics::ToggleActorEquipment(actor, true);
			break;
		default:
			break;
		}

		return EventResult::kContinue;
	}

	EventResult Manager::ProcessEvent(const RE::BGSActorCellEvent* a_evn, RE::BSTEventSource<RE::BGSActorCellEvent>*)
	{
		using CellFlag = RE::BGSActorCellEvent::CellFlag;
		using Type = Settings::ToggleType;

		if (!a_evn || a_evn->flags == CellFlag::kLeave) {
			return EventResult::kContinue;
		}

		const auto cell = RE::TESForm::LookupByID<RE::TESObjectCELL>(a_evn->cellID);
		if (!cell) {
			return EventResult::kContinue;
		}

		bool result = false;
		if (detail::is_cell_home(cell)) {
			playerInHouse = true;
			result = true;
		} else if (playerInHouse) {
			playerInHouse = false;
			result = true;
		}

		if (result) {
			switch (Settings::GetSingleton()->autoToggleType) {
			case Type::kPlayerOnly:
				Graphics::ToggleActorEquipment(RE::PlayerCharacter::GetSingleton(), playerInHouse);
				break;
			case Type::kFollowerOnly:
				Graphics::ToggleFollowerEquipment(playerInHouse);
				break;
			case Type::kPlayerAndFollower:
			case Type::kEveryone:
				{
					Graphics::ToggleActorEquipment(RE::PlayerCharacter::GetSingleton(), playerInHouse);
					Graphics::ToggleFollowerEquipment(playerInHouse);
				}
				break;
			default:
				break;
			}
		}

		return EventResult::kContinue;
	}

	EventResult Manager::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*)
	{
		using InputType = RE::INPUT_EVENT_TYPE;
		using Keyboard = RE::BSWin32KeyboardDevice::Key;
		using Type = Settings::ToggleType;

		if (!a_event || detail::is_in_menu()) {
			return EventResult::kContinue;
		}

		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->Is3DLoaded()) {
			return EventResult::kContinue;
		}

		for (auto event = *a_event; event; event = event->next) {
			if (event->eventType != InputType::kButton) {
				continue;
			}

			const auto button = static_cast<RE::ButtonEvent*>(event);
			if (!button->IsDown() || button->device != RE::INPUT_DEVICE::kKeyboard) {
				continue;
			}

			const auto key = static_cast<Key>(button->idCode);
			detail::process_key(key, armorSlots, weaponSlots, [&](auto& a_slotData) {
				switch (Settings::GetSingleton()->hotkeyToggleType) {
				case Type::kPlayerOnly:
					Graphics::ToggleActorEquipment(player, a_slotData);
					break;
				case Type::kFollowerOnly:
					Graphics::ToggleFollowerEquipment(a_slotData);
					break;
				case Type::kPlayerAndFollower:
					{
						Graphics::ToggleActorEquipment(player, a_slotData);
						Graphics::ToggleFollowerEquipment(a_slotData);
					}
					break;
				case Type::kEveryone:
					{
						Graphics::ToggleActorEquipment(player, a_slotData);
						Graphics::ToggleNPCEquipment(a_slotData);
					}
					break;
				default:
					break;
				}
			});
		}

		return EventResult::kContinue;
	}
}
