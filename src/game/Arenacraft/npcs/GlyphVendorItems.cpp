#include "ItemVendor.hpp"

// Flat, code-defined glyph vendor stock: one row per glyph item, grouped by
// class. The class-dependent "Glyphs" gossip option opens the rows for the
// player's class (see ItemVendor.hpp / ItemVendor.cpp).
//
// Source: WotLK item database, item class 16 ("Glyph of ..."). Within each class
// the glyphs recommended by the Wowhead PvP arena guides come first (captured in
// ai-docs/class_guides.md), then the remaining major glyphs; minor glyphs are
// kept last within their class. Vendor order follows row order, so moving a row
// up moves it up in the vendor.
//
// To add a glyph: append `{CLASS_X, <itemId>},` under its class comment.

namespace arenacraft
{
std::vector<GlyphEntry> const& AllGlyphs()
{
  static std::vector<GlyphEntry> const glyphs = {
      // == Warrior ==
      // Guide-recommended glyphs first (see ai-docs/class_guides.md), then the
      // remaining major glyphs, then minor glyphs; reorder rows to change the
      // vendor order.
      {CLASS_WARRIOR, 45790}, // Glyph of Bladestorm
      {CLASS_WARRIOR, 43417}, // Glyph of Hamstring
      {CLASS_WARRIOR, 43421}, // Glyph of Mortal Strike
      {CLASS_WARRIOR, 43432}, // Glyph of Whirlwind
      {CLASS_WARRIOR, 43418}, // Glyph of Heroic Strike
      {CLASS_WARRIOR, 45794}, // Glyph of Enraged Regeneration
      {CLASS_WARRIOR, 43425}, // Glyph of Blocking
      {CLASS_WARRIOR, 45792}, // Glyph of Shockwave
      {CLASS_WARRIOR, 43415}, // Glyph of Devastate
      {CLASS_WARRIOR, 43424}, // Glyph of Revenge
      {CLASS_WARRIOR, 43420}, // Glyph of Barbaric Insults
      {CLASS_WARRIOR, 43412}, // Glyph of Bloodthirst
      {CLASS_WARRIOR, 43414}, // Glyph of Cleaving
      {CLASS_WARRIOR, 43416}, // Glyph of Execution
      {CLASS_WARRIOR, 43419}, // Glyph of Intervene
      {CLASS_WARRIOR, 43426}, // Glyph of Last Stand
      {CLASS_WARRIOR, 43422}, // Glyph of Overpower
      {CLASS_WARRIOR, 43413}, // Glyph of Rapid Charge
      {CLASS_WARRIOR, 43423}, // Glyph of Rending
      {CLASS_WARRIOR, 43430}, // Glyph of Resonating Power
      {CLASS_WARRIOR, 45797}, // Glyph of Shield Wall
      {CLASS_WARRIOR, 45795}, // Glyph of Spell Reflection
      {CLASS_WARRIOR, 43427}, // Glyph of Sunder Armor
      {CLASS_WARRIOR, 43428}, // Glyph of Sweeping Strikes
      {CLASS_WARRIOR, 43429}, // Glyph of Taunt
      {CLASS_WARRIOR, 43431}, // Glyph of Victory Rush
      {CLASS_WARRIOR, 45793}, // Glyph of Vigilance
      {CLASS_WARRIOR, 43397}, // Glyph of Charge
      {CLASS_WARRIOR, 43399}, // Glyph of Thunder Clap
      {CLASS_WARRIOR, 43396}, // Glyph of Bloodrage
      {CLASS_WARRIOR, 43395}, // Glyph of Battle
      {CLASS_WARRIOR, 49084}, // Glyph of Command
      {CLASS_WARRIOR, 43400}, // Glyph of Enduring Victory
      {CLASS_WARRIOR, 43398}, // Glyph of Mocking Blow
      // == Paladin ==
      // Guide-recommended glyphs first (see ai-docs/class_guides.md), then the
      // remaining major glyphs, then minor glyphs; reorder rows to change the
      // vendor order.
      {CLASS_PALADIN, 45746}, // Glyph of Holy Shock
      {CLASS_PALADIN, 41110}, // Glyph of Seal of Light
      {CLASS_PALADIN, 41102}, // Glyph of Turn Evil
      {CLASS_PALADIN, 41092}, // Glyph of Judgement
      {CLASS_PALADIN, 45747}, // Glyph of Salvation
      {CLASS_PALADIN, 41101}, // Glyph of Avenger's Shield
      {CLASS_PALADIN, 41107}, // Glyph of Avenging Wrath
      {CLASS_PALADIN, 45741}, // Glyph of Beacon of Light
      {CLASS_PALADIN, 41104}, // Glyph of Cleansing
      {CLASS_PALADIN, 41099}, // Glyph of Consecration
      {CLASS_PALADIN, 41098}, // Glyph of Crusader Strike
      {CLASS_PALADIN, 45745}, // Glyph of Divine Plea
      {CLASS_PALADIN, 45743}, // Glyph of Divine Storm
      {CLASS_PALADIN, 41108}, // Glyph of Divinity
      {CLASS_PALADIN, 41103}, // Glyph of Exorcism
      {CLASS_PALADIN, 41105}, // Glyph of Flash of Light
      {CLASS_PALADIN, 41095}, // Glyph of Hammer of Justice
      {CLASS_PALADIN, 45742}, // Glyph of Hammer of the Righteous
      {CLASS_PALADIN, 41097}, // Glyph of Hammer of Wrath
      {CLASS_PALADIN, 41106}, // Glyph of Holy Light
      {CLASS_PALADIN, 43867}, // Glyph of Holy Wrath
      {CLASS_PALADIN, 41100}, // Glyph of Righteous Defense
      {CLASS_PALADIN, 41094}, // Glyph of Seal of Command
      {CLASS_PALADIN, 43868}, // Glyph of Seal of Righteousness
      {CLASS_PALADIN, 43869}, // Glyph of Seal of Vengeance
      {CLASS_PALADIN, 41109}, // Glyph of Seal of Wisdom
      {CLASS_PALADIN, 45744}, // Glyph of Shield of Righteousness
      {CLASS_PALADIN, 41096}, // Glyph of Spiritual Attunement
      {CLASS_PALADIN, 43367}, // Glyph of Lay on Hands
      {CLASS_PALADIN, 43365}, // Glyph of Blessing of Kings
      {CLASS_PALADIN, 43368}, // Glyph of Sense Undead
      {CLASS_PALADIN, 43369}, // Glyph of the Wise
      {CLASS_PALADIN, 43340}, // Glyph of Blessing of Might
      {CLASS_PALADIN, 43366}, // Glyph of Blessing of Wisdom
      // == Hunter ==
      // Guide-recommended glyphs first (see ai-docs/class_guides.md), then the
      // remaining major glyphs, then minor glyphs; reorder rows to change the
      // vendor order.
      {CLASS_HUNTER, 42897}, // Glyph of Aimed Shot
      {CLASS_HUNTER, 42902}, // Glyph of Bestial Wrath
      {CLASS_HUNTER, 42904}, // Glyph of Disengage
      {CLASS_HUNTER, 42912}, // Glyph of Serpent Sting
      {CLASS_HUNTER, 45625}, // Glyph of Chimera Shot
      {CLASS_HUNTER, 45731}, // Glyph of Explosive Shot
      {CLASS_HUNTER, 42898}, // Glyph of Arcane Shot
      {CLASS_HUNTER, 42901}, // Glyph of Aspect of the Viper
      {CLASS_HUNTER, 42903}, // Glyph of Deterrence
      {CLASS_HUNTER, 45733}, // Glyph of Explosive Trap
      {CLASS_HUNTER, 42905}, // Glyph of Freezing Trap
      {CLASS_HUNTER, 42906}, // Glyph of Frost Trap
      {CLASS_HUNTER, 42907}, // Glyph of Hunter's Mark
      {CLASS_HUNTER, 42908}, // Glyph of Immolation Trap
      {CLASS_HUNTER, 45732}, // Glyph of Kill Shot
      {CLASS_HUNTER, 42900}, // Glyph of Mending
      {CLASS_HUNTER, 42910}, // Glyph of Multi-Shot
      {CLASS_HUNTER, 42911}, // Glyph of Rapid Fire
      {CLASS_HUNTER, 45735}, // Glyph of Raptor Strike
      {CLASS_HUNTER, 45734}, // Glyph of Scatter Shot
      {CLASS_HUNTER, 42913}, // Glyph of Snake Trap
      {CLASS_HUNTER, 42914}, // Glyph of Steady Shot
      {CLASS_HUNTER, 42899}, // Glyph of the Beast
      {CLASS_HUNTER, 42909}, // Glyph of the Hawk
      {CLASS_HUNTER, 42915}, // Glyph of Trueshot Aura
      {CLASS_HUNTER, 42916}, // Glyph of Volley
      {CLASS_HUNTER, 42917}, // Glyph of Wyvern Sting
      {CLASS_HUNTER, 43351}, // Glyph of Feign Death
      {CLASS_HUNTER, 43350}, // Glyph of Mend Pet
      {CLASS_HUNTER, 43338}, // Glyph of Revive Pet
      {CLASS_HUNTER, 43356}, // Glyph of Scare Beast
      {CLASS_HUNTER, 43354}, // Glyph of Possessed Strength
      {CLASS_HUNTER, 43355}, // Glyph of the Pack
      // == Rogue ==
      // Guide-recommended glyphs first (see ai-docs/class_guides.md), then the
      // remaining major glyphs, then minor glyphs; reorder rows to change the
      // vendor order.
      {CLASS_ROGUE, 45768}, // Glyph of Mutilate
      {CLASS_ROGUE, 42971}, // Glyph of Vigor
      {CLASS_ROGUE, 45767}, // Glyph of Tricks of the Trade
      {CLASS_ROGUE, 42974}, // Glyph of Sprint
      {CLASS_ROGUE, 42968}, // Glyph of Preparation
      {CLASS_ROGUE, 45762}, // Glyph of Killing Spree
      {CLASS_ROGUE, 42972}, // Glyph of Sinister Strike
      {CLASS_ROGUE, 45764}, // Glyph of Shadow Dance
      {CLASS_ROGUE, 45769}, // Glyph of Cloak of Shadows
      {CLASS_ROGUE, 42954}, // Glyph of Adrenaline Rush
      {CLASS_ROGUE, 42955}, // Glyph of Ambush
      {CLASS_ROGUE, 42956}, // Glyph of Backstab
      {CLASS_ROGUE, 42957}, // Glyph of Blade Flurry
      {CLASS_ROGUE, 42958}, // Glyph of Crippling Poison
      {CLASS_ROGUE, 42959}, // Glyph of Deadly Throw
      {CLASS_ROGUE, 45908}, // Glyph of Envenom
      {CLASS_ROGUE, 42960}, // Glyph of Evasion
      {CLASS_ROGUE, 42961}, // Glyph of Eviscerate
      {CLASS_ROGUE, 42962}, // Glyph of Expose Armor
      {CLASS_ROGUE, 45766}, // Glyph of Fan of Knives
      {CLASS_ROGUE, 42963}, // Glyph of Feint
      {CLASS_ROGUE, 42964}, // Glyph of Garrote
      {CLASS_ROGUE, 42965}, // Glyph of Ghostly Strike
      {CLASS_ROGUE, 42966}, // Glyph of Gouge
      {CLASS_ROGUE, 42967}, // Glyph of Hemorrhage
      {CLASS_ROGUE, 45761}, // Glyph of Hunger for Blood
      {CLASS_ROGUE, 42969}, // Glyph of Rupture
      {CLASS_ROGUE, 42970}, // Glyph of Sap
      {CLASS_ROGUE, 42973}, // Glyph of Slice and Dice
      {CLASS_ROGUE, 43376}, // Glyph of Distract
      {CLASS_ROGUE, 43380}, // Glyph of Vanish
      {CLASS_ROGUE, 43379}, // Glyph of Blurred Speed
      {CLASS_ROGUE, 43378}, // Glyph of Safe Fall
      {CLASS_ROGUE, 43377}, // Glyph of Pick Lock
      {CLASS_ROGUE, 43343}, // Glyph of Pick Pocket
      // == Priest ==
      // Guide-recommended glyphs first (see ai-docs/class_guides.md), then the
      // remaining major glyphs, then minor glyphs; reorder rows to change the
      // vendor order.
      {CLASS_PRIEST, 45756}, // Glyph of Penance
      {CLASS_PRIEST, 45760}, // Glyph of Pain Suppression
      {CLASS_PRIEST, 42408}, // Glyph of Power Word: Shield
      {CLASS_PRIEST, 45755}, // Glyph of Guardian Spirit
      {CLASS_PRIEST, 42417}, // Glyph of Spirit of Redemption
      {CLASS_PRIEST, 45753}, // Glyph of Dispersion
      {CLASS_PRIEST, 42398}, // Glyph of Fade
      {CLASS_PRIEST, 42396}, // Glyph of Circle of Healing
      {CLASS_PRIEST, 42397}, // Glyph of Dispel Magic
      {CLASS_PRIEST, 42399}, // Glyph of Fear Ward
      {CLASS_PRIEST, 42400}, // Glyph of Flash Heal
      {CLASS_PRIEST, 42401}, // Glyph of Holy Nova
      {CLASS_PRIEST, 45758}, // Glyph of Hymn of Hope
      {CLASS_PRIEST, 42402}, // Glyph of Inner Fire
      {CLASS_PRIEST, 42403}, // Glyph of Lightwell
      {CLASS_PRIEST, 42404}, // Glyph of Mass Dispel
      {CLASS_PRIEST, 42405}, // Glyph of Mind Control
      {CLASS_PRIEST, 42415}, // Glyph of Mind Flay
      {CLASS_PRIEST, 45757}, // Glyph of Mind Sear
      {CLASS_PRIEST, 42409}, // Glyph of Prayer of Healing
      {CLASS_PRIEST, 42410}, // Glyph of Psychic Scream
      {CLASS_PRIEST, 42411}, // Glyph of Renew
      {CLASS_PRIEST, 42412}, // Glyph of Scourge Imprisonment
      {CLASS_PRIEST, 42407}, // Glyph of Shadow
      {CLASS_PRIEST, 42414}, // Glyph of Shadow Word: Death
      {CLASS_PRIEST, 42406}, // Glyph of Shadow Word: Pain
      {CLASS_PRIEST, 42416}, // Glyph of Smite
      {CLASS_PRIEST, 43374}, // Glyph of Shadowfiend
      {CLASS_PRIEST, 43371}, // Glyph of Fortitude
      {CLASS_PRIEST, 43373}, // Glyph of Shackle Undead
      {CLASS_PRIEST, 43370}, // Glyph of Levitate
      {CLASS_PRIEST, 43342}, // Glyph of Fading
      {CLASS_PRIEST, 43372}, // Glyph of Shadow Protection
      // == Death Knight ==
      // Guide-recommended glyphs first (see ai-docs/class_guides.md), then the
      // remaining major glyphs, then minor glyphs; reorder rows to change the
      // vendor order.
      {CLASS_DEATH_KNIGHT, 45799}, // Glyph of Dancing Rune Weapon
      {CLASS_DEATH_KNIGHT, 43537}, // Glyph of Chains of Ice
      {CLASS_DEATH_KNIGHT, 45805}, // Glyph of Disease
      {CLASS_DEATH_KNIGHT, 44432}, // Glyph of Raise Dead
      {CLASS_DEATH_KNIGHT, 45806}, // Glyph of Howling Blast
      {CLASS_DEATH_KNIGHT, 43543}, // Glyph of Frost Strike
      {CLASS_DEATH_KNIGHT, 45800}, // Glyph of Hungering Cold
      {CLASS_DEATH_KNIGHT, 43533}, // Glyph of Anti-Magic Shell
      {CLASS_DEATH_KNIGHT, 45804}, // Glyph of Dark Death
      {CLASS_DEATH_KNIGHT, 43549}, // Glyph of the Ghoul
      {CLASS_DEATH_KNIGHT, 43826}, // Glyph of Blood Strike
      {CLASS_DEATH_KNIGHT, 43536}, // Glyph of Bone Shield
      {CLASS_DEATH_KNIGHT, 43538}, // Glyph of Dark Command
      {CLASS_DEATH_KNIGHT, 43542}, // Glyph of Death and Decay
      {CLASS_DEATH_KNIGHT, 43541}, // Glyph of Death Grip
      {CLASS_DEATH_KNIGHT, 43827}, // Glyph of Death Strike
      {CLASS_DEATH_KNIGHT, 43534}, // Glyph of Heart Strike
      {CLASS_DEATH_KNIGHT, 43545}, // Glyph of Icebound Fortitude
      {CLASS_DEATH_KNIGHT, 43546}, // Glyph of Icy Touch
      {CLASS_DEATH_KNIGHT, 43547}, // Glyph of Obliterate
      {CLASS_DEATH_KNIGHT, 43548}, // Glyph of Plague Strike
      {CLASS_DEATH_KNIGHT, 43550}, // Glyph of Rune Strike
      {CLASS_DEATH_KNIGHT, 43825}, // Glyph of Rune Tap
      {CLASS_DEATH_KNIGHT, 43551}, // Glyph of Scourge Strike
      {CLASS_DEATH_KNIGHT, 43552}, // Glyph of Strangulate
      {CLASS_DEATH_KNIGHT, 43553}, // Glyph of Unbreakable Armor
      {CLASS_DEATH_KNIGHT, 45803}, // Glyph of Unholy Blight
      {CLASS_DEATH_KNIGHT, 43554}, // Glyph of Vampiric Blood
      {CLASS_DEATH_KNIGHT, 43535}, // Glyph of Blood Tap
      {CLASS_DEATH_KNIGHT, 43672}, // Glyph of Pestilence
      {CLASS_DEATH_KNIGHT, 43539}, // Glyph of Death's Embrace
      {CLASS_DEATH_KNIGHT, 43671}, // Glyph of Corpse Explosion
      {CLASS_DEATH_KNIGHT, 43544}, // Glyph of Horn of Winter
      {CLASS_DEATH_KNIGHT, 43673}, // Glyph of Raise Dead
      // == Shaman ==
      // Guide-recommended glyphs first (see ai-docs/class_guides.md), then the
      // remaining major glyphs, then minor glyphs; reorder rows to change the
      // vendor order.
      {CLASS_SHAMAN, 45770}, // Glyph of Thunder
      {CLASS_SHAMAN, 41524}, // Glyph of Lava
      {CLASS_SHAMAN, 45778}, // Glyph of Stoneclaw Totem
      {CLASS_SHAMAN, 41526}, // Glyph of Shocking
      {CLASS_SHAMAN, 45771}, // Glyph of Feral Spirit
      {CLASS_SHAMAN, 45775}, // Glyph of Earth Shield
      {CLASS_SHAMAN, 41534}, // Glyph of Healing Wave
      {CLASS_SHAMAN, 41517}, // Glyph of Chain Heal
      {CLASS_SHAMAN, 41518}, // Glyph of Chain Lightning
      {CLASS_SHAMAN, 41527}, // Glyph of Earthliving Weapon
      {CLASS_SHAMAN, 41552}, // Glyph of Elemental Mastery
      {CLASS_SHAMAN, 41529}, // Glyph of Fire Elemental Totem
      {CLASS_SHAMAN, 41530}, // Glyph of Fire Nova
      {CLASS_SHAMAN, 41531}, // Glyph of Flame Shock
      {CLASS_SHAMAN, 41532}, // Glyph of Flametongue Weapon
      {CLASS_SHAMAN, 41547}, // Glyph of Frost Shock
      {CLASS_SHAMAN, 41533}, // Glyph of Healing Stream Totem
      {CLASS_SHAMAN, 45777}, // Glyph of Hex
      {CLASS_SHAMAN, 41540}, // Glyph of Lava Lash
      {CLASS_SHAMAN, 41535}, // Glyph of Lesser Healing Wave
      {CLASS_SHAMAN, 41536}, // Glyph of Lightning Bolt
      {CLASS_SHAMAN, 41537}, // Glyph of Lightning Shield
      {CLASS_SHAMAN, 41538}, // Glyph of Mana Tide Totem
      {CLASS_SHAMAN, 45772}, // Glyph of Riptide
      {CLASS_SHAMAN, 41539}, // Glyph of Stormstrike
      {CLASS_SHAMAN, 45776}, // Glyph of Totem of Wrath
      {CLASS_SHAMAN, 41541}, // Glyph of Water Mastery
      {CLASS_SHAMAN, 41542}, // Glyph of Windfury Weapon
      {CLASS_SHAMAN, 43386}, // Glyph of Water Shield
      {CLASS_SHAMAN, 43725}, // Glyph of Ghost Wolf
      {CLASS_SHAMAN, 43388}, // Glyph of Water Walking
      {CLASS_SHAMAN, 43381}, // Glyph of Astral Recall
      {CLASS_SHAMAN, 43385}, // Glyph of Renewed Life
      {CLASS_SHAMAN, 44923}, // Glyph of Thunderstorm
      {CLASS_SHAMAN, 43344}, // Glyph of Water Breathing
      // == Mage ==
      // Guide-recommended glyphs first (see ai-docs/class_guides.md), then the
      // remaining major glyphs, then minor glyphs; reorder rows to change the
      // vendor order.
      {CLASS_MAGE, 42735}, // Glyph of Arcane Missiles
      {CLASS_MAGE, 42738}, // Glyph of Evocation
      {CLASS_MAGE, 44955}, // Glyph of Arcane Blast
      {CLASS_MAGE, 45737}, // Glyph of Living Bomb
      {CLASS_MAGE, 42752}, // Glyph of Polymorph
      {CLASS_MAGE, 45740}, // Glyph of Ice Barrier
      {CLASS_MAGE, 45738}, // Glyph of Arcane Barrage
      {CLASS_MAGE, 42734}, // Glyph of Arcane Explosion
      {CLASS_MAGE, 42736}, // Glyph of Arcane Power
      {CLASS_MAGE, 42737}, // Glyph of Blink
      {CLASS_MAGE, 45736}, // Glyph of Deep Freeze
      {CLASS_MAGE, 50045}, // Glyph of Eternal Water
      {CLASS_MAGE, 42740}, // Glyph of Fire Blast
      {CLASS_MAGE, 42739}, // Glyph of Fireball
      {CLASS_MAGE, 42741}, // Glyph of Frost Nova
      {CLASS_MAGE, 42742}, // Glyph of Frostbolt
      {CLASS_MAGE, 44684}, // Glyph of Frostfire
      {CLASS_MAGE, 42743}, // Glyph of Ice Armor
      {CLASS_MAGE, 42744}, // Glyph of Ice Block
      {CLASS_MAGE, 42745}, // Glyph of Ice Lance
      {CLASS_MAGE, 42746}, // Glyph of Icy Veins
      {CLASS_MAGE, 42748}, // Glyph of Invisibility
      {CLASS_MAGE, 42749}, // Glyph of Mage Armor
      {CLASS_MAGE, 42750}, // Glyph of Mana Gem
      {CLASS_MAGE, 45739}, // Glyph of Mirror Image
      {CLASS_MAGE, 42751}, // Glyph of Molten Armor
      {CLASS_MAGE, 42753}, // Glyph of Remove Curse
      {CLASS_MAGE, 42747}, // Glyph of Scorch
      {CLASS_MAGE, 42754}, // Glyph of Water Elemental
      {CLASS_MAGE, 43339}, // Glyph of Arcane Intellect
      {CLASS_MAGE, 43357}, // Glyph of Fire Ward
      {CLASS_MAGE, 43360}, // Glyph of Frost Ward
      {CLASS_MAGE, 44920}, // Glyph of Blast Wave
      {CLASS_MAGE, 43359}, // Glyph of Frost Armor
      {CLASS_MAGE, 43364}, // Glyph of Slow Fall
      {CLASS_MAGE, 43362}, // Glyph of the Bear Cub
      {CLASS_MAGE, 43361}, // Glyph of the Penguin
      // == Warlock ==
      // Guide-recommended glyphs first (see ai-docs/class_guides.md), then the
      // remaining major glyphs, then minor glyphs; reorder rows to change the
      // vendor order.
      {CLASS_WARLOCK, 45783}, // Glyph of Shadowflame
      {CLASS_WARLOCK, 50077}, // Glyph of Quick Decay
      {CLASS_WARLOCK, 42469}, // Glyph of Siphon Life
      {CLASS_WARLOCK, 45789}, // Glyph of Soul Link
      {CLASS_WARLOCK, 45780}, // Glyph of Metamorphosis
      {CLASS_WARLOCK, 42459}, // Glyph of Felguard
      {CLASS_WARLOCK, 42455}, // Glyph of Corruption
      {CLASS_WARLOCK, 42454}, // Glyph of Conflagrate
      {CLASS_WARLOCK, 42458}, // Glyph of Fear
      {CLASS_WARLOCK, 42453}, // Glyph of Incinerate
      {CLASS_WARLOCK, 45781}, // Glyph of Chaos Bolt
      {CLASS_WARLOCK, 42456}, // Glyph of Curse of Agony
      {CLASS_WARLOCK, 42457}, // Glyph of Death Coil
      {CLASS_WARLOCK, 45782}, // Glyph of Demonic Circle
      {CLASS_WARLOCK, 42460}, // Glyph of Felhunter
      {CLASS_WARLOCK, 45779}, // Glyph of Haunt
      {CLASS_WARLOCK, 42461}, // Glyph of Health Funnel
      {CLASS_WARLOCK, 42462}, // Glyph of Healthstone
      {CLASS_WARLOCK, 42463}, // Glyph of Howl of Terror
      {CLASS_WARLOCK, 42464}, // Glyph of Immolate
      {CLASS_WARLOCK, 42465}, // Glyph of Imp
      {CLASS_WARLOCK, 45785}, // Glyph of Life Tap
      {CLASS_WARLOCK, 42466}, // Glyph of Searing Pain
      {CLASS_WARLOCK, 42467}, // Glyph of Shadow Bolt
      {CLASS_WARLOCK, 42468}, // Glyph of Shadowburn
      {CLASS_WARLOCK, 42470}, // Glyph of Soulstone
      {CLASS_WARLOCK, 42471}, // Glyph of Succubus
      {CLASS_WARLOCK, 42472}, // Glyph of Unstable Affliction
      {CLASS_WARLOCK, 42473}, // Glyph of Voidwalker
      {CLASS_WARLOCK, 43390}, // Glyph of Drain Soul
      {CLASS_WARLOCK, 43392}, // Glyph of Curse of Exhaustion
      {CLASS_WARLOCK, 43389}, // Glyph of Unending Breath
      {CLASS_WARLOCK, 43393}, // Glyph of Enslave Demon
      {CLASS_WARLOCK, 43391}, // Glyph of Kilrogg
      {CLASS_WARLOCK, 43394}, // Glyph of Souls
      // == Druid ==
      // Guide-recommended glyphs first (see ai-docs/class_guides.md), then the
      // remaining major glyphs, then minor glyphs; reorder rows to change the
      // vendor order.
      {CLASS_DRUID, 45622}, // Glyph of Monsoon
      {CLASS_DRUID, 40908}, // Glyph of Innervate
      {CLASS_DRUID, 40921}, // Glyph of Starfall
      {CLASS_DRUID, 40919}, // Glyph of Insect Swarm
      {CLASS_DRUID, 40901}, // Glyph of Shred
      {CLASS_DRUID, 40902}, // Glyph of Rip
      {CLASS_DRUID, 45604}, // Glyph of Savage Roar
      {CLASS_DRUID, 45623}, // Glyph of Barkskin
      {CLASS_DRUID, 40906}, // Glyph of Swiftmend
      {CLASS_DRUID, 40913}, // Glyph of Rejuvenation
      {CLASS_DRUID, 45601}, // Glyph of Berserk
      {CLASS_DRUID, 48720}, // Glyph of Claw
      {CLASS_DRUID, 40924}, // Glyph of Entangling Roots
      {CLASS_DRUID, 44928}, // Glyph of Focus
      {CLASS_DRUID, 40896}, // Glyph of Frenzied Regeneration
      {CLASS_DRUID, 40899}, // Glyph of Growl
      {CLASS_DRUID, 40914}, // Glyph of Healing Touch
      {CLASS_DRUID, 40920}, // Glyph of Hurricane
      {CLASS_DRUID, 40915}, // Glyph of Lifebloom
      {CLASS_DRUID, 40900}, // Glyph of Mangle
      {CLASS_DRUID, 40897}, // Glyph of Maul
      {CLASS_DRUID, 40923}, // Glyph of Moonfire
      {CLASS_DRUID, 45603}, // Glyph of Nourish
      {CLASS_DRUID, 40903}, // Glyph of Rake
      {CLASS_DRUID, 50125}, // Glyph of Rapid Rejuvenation
      {CLASS_DRUID, 40909}, // Glyph of Rebirth
      {CLASS_DRUID, 40912}, // Glyph of Regrowth
      {CLASS_DRUID, 40916}, // Glyph of Starfire
      {CLASS_DRUID, 46372}, // Glyph of Survival Instincts
      {CLASS_DRUID, 45602}, // Glyph of Wild Growth
      {CLASS_DRUID, 40922}, // Glyph of Wrath
      {CLASS_DRUID, 43332}, // Glyph of Thorns
      {CLASS_DRUID, 43335}, // Glyph of the Wild
      {CLASS_DRUID, 43674}, // Glyph of Dash
      {CLASS_DRUID, 43316}, // Glyph of Aquatic Form
      {CLASS_DRUID, 43334}, // Glyph of Challenging Roar
      {CLASS_DRUID, 44922}, // Glyph of Typhoon
      {CLASS_DRUID, 43331}, // Glyph of Unburdened Rebirth
  };
  return glyphs;
}
} // namespace arenacraft
