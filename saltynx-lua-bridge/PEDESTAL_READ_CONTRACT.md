# Pedestal read candidate 0.5.0

Exact local input SHA256 CC363208C220F65547EDDA6D8268B3B6C0F7373B14C91AE6FFB378018DE4B66A.
No new gameplay function calls, game writes or hook sites are added.

Read chain, derived from exact-build assembly:

- IsaacUpdate and Game::IsPaused load the manager pointer through NRO GOT +0xAAC648.
- Manager::Update reads manager+8 for state. Its 5-byte switch at +0x8BF814 maps state 2 to +0x3F9030, the Game::Update route.
- That route loads the game through GOT +0xAAC698. Manager::Update +0x3F909C..0x3F90B8 and Level::Update +0x3D08A4..0x3D08B0 use game+0x21550 as current Room*.
- Room::Update preserves its receiver in x19 at +0x46D7C8 and passes room+0x1950 to EntityList::Update at +0x46E6C8. The independently checked PLT maps to EntityList::Update.
- EntityList::CountType +0x7A00C..0x7A150 loads the pointer array at list+0x78, count at list+0x84, and compares type/variant/subtype at entity+0x38/+0x3C/+0x40.

FNV64 guards over full Manager::Update, Room::Update, EntityList::CountType and the state switch detect drift after the existing Build ID/instruction validation. They are not cryptographic authentication; the accompanying manifest records source SHA256 hashes. The generator rejects runtime relocations in guarded ranges.

All pointer ranges are checked against readable mappings with overflow bounds.
Maximum 512 entities and 128 positive collectible subtypes; overflow returns nil/error.
Reads run after the original update on the existing serialized Lua owner. Lua polls every 15 callbacks.

Type 5 / variant 100 identify pickup/collectible candidates using the Workshop enum convention. This mapping and the live pointer chain still need the first device observation. No item quality, curse visibility, shop/choice eligibility or removal flags are assumed. An ID observed here is NOT yet a valid Specialist trigger.

No native costume function is invoked until its resources, numeric IDs and live player receiver have a separate verified contract.
