local t = {
	app = { name = "rpcs3", desc = "RPCS3 - Playstation 3 Emulator" },
	emu = { desc = "Dragon Quest Builders" },
	endian = "Big",
	list = {
{ desc = "Player", addr = 18036876676,
	list = { 
		{ desc = "Days", offset = 0, Type = "signed", size = 2 },
		{ desc = "HP Upto", offset = 2, Type = "signed", size = 2 },
		{ desc = "HP Left", offset = 4, Type = "signed", size = 2 },
		{ desc = "Hunger Upto", offset = 6, Type = "signed", size = 2 },
		{ desc = "Hunger Left", offset = 8, Type = "signed", size = 2 },
		{ desc = "Attack", offset = 10, Type = "signed", size = 2 },
		{ desc = "Defence", offset = 12, Type = "signed", size = 2 }
	}
},
{ desc = "???", addr = 18036876692, Type = "signed", size = 2 },
{ desc = "Items", addr = 18036976864,
	count = 15, size = 4, split = {
		{desc="ID", use = "items", Type = "bytes", size = 2},
		{desc="Qty",Type = "signed", size = 2} } },
{ desc = "Gear", addr = 18036976924,
	count = 16, size = 4, split = {
		{desc="ID", use = "items", Type = "bytes", size = 2},
		{desc="HP",Type = "signed", size = 2} } }
	}
}
t.usable = {}
t.usable.items = {
	{ id = "DQ B1", desc = "Item List (Reverse Bytes)" },
	{ id = "00 00", desc = "Healing Cream" },
	{ id = "00 01", desc = "Block" },
	{ id = "00 02", desc = "Block" },
	{ id = "00 03", desc = "Block" },
	{ id = "00 04", desc = "Molten Rock Block" },
	{ id = "00 05", desc = "Bonfire" },
	{ id = "00 06", desc = "Old Note" },
	{ id = "00 07", desc = "Block" },
	{ id = "00 08", desc = "Crash" },
	{ id = "00 09", desc = "Holy Water" },
	{ id = "00 0A", desc = "Block" },
	{ id = "00 0B", desc = "Chimera Wing" },
	{ id = "00 0C", desc = "Block" },
	{ id = "00 0D", desc = "Seed of Life" },
	{ id = "00 0E", desc = "Block" },
	{ id = "00 10", desc = "Defuddle Drops" },
	{ id = "00 11", desc = "Tingle Tablet" },
	{ id = "00 12", desc = "Block" },
	{ id = "00 13", desc = "Block" },
	{ id = "00 14", desc = "Throwing Stone" },
	{ id = "00 15", desc = "Not Used (Boomarang, Throws Fireball)" },
	{ id = "00 16", desc = "Not Used (Poison Needle, Throws Fireball)" },
	{ id = "00 17", desc = "Not Used (Throwing Knife?)" },
	{ id = "00 18", desc = "Divine Dagger (Thrown)" },
	{ id = "00 19", desc = "Block" },
	{ id = "00 1A", desc = "Not Used (Wooden Bucket)" },
	{ id = "00 1B", desc = "Not Used (Iron? Bucket)" },
	{ id = "00 1C", desc = "Not Used (Obsidian? Bucket)" },
	{ id = "00 1D", desc = "Rake" },
	{ id = "00 1E", desc = "Not Used (Grass? Seed - Cannot put on tilled soil)" },
	{ id = "00 20", desc = "Fishing Rod" },
	{ id = "00 21", desc = "Sizz Shot" },
	{ id = "00 22", desc = "Crack Shot" },
	{ id = "00 23", desc = "Key" },
	{ id = "00 24", desc = "Ultimate Key" },
	{ id = "00 25", desc = "Rainbow Drop" },
	{ id = "00 26", desc = "Kaboom Shot" },
	{ id = "00 27", desc = "Tranished Token" },
	{ id = "00 28", desc = "Storm Stone" },
	{ id = "00 29", desc = "Cracked Crystal" },
	{ id = "00 2A", desc = "Block" },
	{ id = "00 2B", desc = "Block" },
	{ id = "00 2C", desc = "Staff of Rain (Does nothing when used)" },
	{ id = "00 2D", desc = "Sunstone (Does nothing when used)" },
	{ id = "00 2E", desc = "Block" },
	{ id = "00 30", desc = "Horn Rimmed Bucket" },
	{ id = "00 31", desc = "Block" },
	{ id = "00 32", desc = "Sheen Salts" },
	{ id = "00 33", desc = "Block" },
	{ id = "00 34", desc = "World Map" },
	{ id = "00 35", desc = "Block" },
	{ id = "00 36", desc = "Wheat Seed" },
	{ id = "00 37", desc = "Potato Sprout" },
	{ id = "00 38", desc = "Heartfruit Seed" },
	{ id = "00 39", desc = "Sugar Cane Seedling" },
	{ id = "00 3A", desc = "Butterbeen Sprout" },
	{ id = "00 3B", desc = "Faire Firtiliser" },
	{ id = "00 3C", desc = "Block" },
	{ id = "00 3D", desc = "Sphere of Darkness" },
	{ id = "00 3E", desc = "Sphere of Light (Does nothing when used)" },
	{ id = "00 40", desc = "Not Used (Holy Water Variant?)" },
	{ id = "00 41", desc = "Ancient Emblem" },
	{ id = "00 42", desc = "Staff of Rain" },
	{ id = "00 43", desc = "Sunstone" },
	{ id = "00 44", desc = "Not Used (Smiley Sun)" },
	{ id = "00 45", desc = "Confetti" },
	{ id = "00 46", desc = "Slimey Tussle Ticket" },
	{ id = "00 47", desc = "Flattened Tussle Ticket" },
	{ id = "00 48", desc = "Poisoness Tussle Ticket" },
	{ id = "00 49", desc = "Roughed Up Tussle Ticket" },
	{ id = "00 4A", desc = "Torrid Tussle Ticket" },
	{ id = "00 4B", desc = "Magical Tussle Ticket" },
	{ id = "00 4C", desc = "Bloody Tussle Ticket" },
	{ id = "00 4D", desc = "Demonic Tussle Ticket" },
	{ id = "00 4E", desc = "Skeletal Trouble Ticket" },
	{ id = "00 50", desc = "Prickley Trouble Ticket" },
	{ id = "00 51", desc = "Rotten Trouble Ticket" },
	{ id = "00 52", desc = "Trickey Trouble Ticket" },
	{ id = "00 53", desc = "Metalic Trouble Ticket" },
	{ id = "00 54", desc = "Baleful Trouble Ticket" },
	{ id = "00 55", desc = "Deadly Trouble Ticket" },
	{ id = "00 56", desc = "Stony Trauma Ticket" },
	{ id = "00 57", desc = "Frightening Trauma Ticket" },
	{ id = "00 58", desc = "Piercing Trauma Ticket" },
	{ id = "00 59", desc = "Respendant Trauma Ticket" },
	{ id = "00 5A", desc = "Towering Trauma Ticket" },
	{ id = "00 5B", desc = "Mechanical Trauma Ticket" },
	{ id = "00 5C", desc = "Burning Trauma Ticket" },
	{ id = "00 5D", desc = "Dreadful Trauma Ticket" },
	{ id = "00 5E", desc = "All Knighter Ticket" },
	{ id = "00 60", desc = "Gold Rush Ticket" },
	{ id = "00 61", desc = "Furry Fury Ticket" },
	{ id = "00 62", desc = "Mager League Ticket" },
	{ id = "00 63", desc = "Blackened Bronze Ticket" },
	{ id = "00 64", desc = "Blank Ticket" },
	{ id = "00 65", desc = "Not Used (Acorn Filled +5%)" },
	{ id = "00 66", desc = "Fruit Salad" },
	{ id = "00 67", desc = "Not Used (Pumpkin - Filled +20%)" },
	{ id = "00 68", desc = "Not Used (Premium Steak - Filled +100%, HP +30)" },
	{ id = "00 69", desc = "Not Used (Steak - Filled +20%)" },
	{ id = "00 6A", desc = "Not Used (Bread - Filled +30%)" },
	{ id = "00 6B", desc = "Not Used (Gold Block)" },
	{ id = "00 6C", desc = "Not Used (Gold Block)" },
	{ id = "00 6D", desc = "Not Used (Gold Block)" },
	{ id = "00 6E", desc = "Not Used (Pineaple Piece? - Filled +20%)" },
	{ id = "00 70", desc = "Cactus Steak" },
	{ id = "00 71", desc = "Block" },
	{ id = "00 72", desc = "Not Used (Gold Block - Filled +50%)" },
	{ id = "00 73", desc = "Not Used (Gold Block - Filled +50%)" },
	{ id = "00 74", desc = "Sardine-on-a-stick" },
	{ id = "00 75", desc = "Sautoed Salmon" },
	{ id = "00 76", desc = "Not Used (Juice - Filled +50%)" },
	{ id = "00 77", desc = "Snow Cone" },
	{ id = "00 78", desc = "Block" },
	{ id = "00 79", desc = "Not Used (Yellow Scorpian - Filled +40%)" },
	{ id = "00 7A", desc = "Boullatisse (Filled +100%)" },
	{ id = "00 7B", desc = "Boiled Butterbeans (Filled +10%)" },
	{ id = "00 7C", desc = "Plumerry (Filled +10%)" },
	{ id = "00 7D", desc = "Block" },
	{ id = "00 7E", desc = "Plumerry Jam (Filled +20%)" },
	{ id = "00 80", desc = "Jam Doughnuts (Filled +80%)" },
	{ id = "00 81", desc = "Block" },
	{ id = "00 82", desc = "Bunicorn Steak (Filled +20%)" },
	{ id = "00 83", desc = "Bunny Burger (Filled +80%)" },
	{ id = "00 84", desc = "Potato Salad (Filled +30%, HP +20)" },
	{ id = "00 85", desc = "Super Salad (Filled +80%, HP +50)" },
	{ id = "00 86", desc = "Beany Buny Burger (Filled +100%)" },
	{ id = "00 87", desc = "Block" },
	{ id = "00 88", desc = "Bunny Bits (Filled +60%)" },
	{ id = "00 89", desc = "Black & Blue Steak (Filled +40%)" },
	{ id = "00 8A", desc = "Searing Steak (Filled +50%)" },
	{ id = "00 8B", desc = "Coddled Egg (Filled +30%)" },
	{ id = "00 8C", desc = "Fried Egg (Filled +20%)" },
	{ id = "00 8D", desc = "Block" },
	{ id = "00 8E", desc = "Ice cream (Filled +20%, HP +50)" },
	{ id = "00 90", desc = "Fried Frogstool (Filled +20%)" },
	{ id = "00 91", desc = "Block" },
	{ id = "00 92", desc = "Block" },
	{ id = "00 93", desc = "Block" },
	{ id = "00 94", desc = "Block" },
	{ id = "00 95", desc = "Block" },
	{ id = "00 96", desc = "Brick Cladding" },
	{ id = "00 97", desc = "Flagstone Flooring" },
	{ id = "00 98", desc = "Stone Cladding" },
	{ id = "00 99", desc = "Straw Flooring" },
	{ id = "00 9A", desc = "Wooden Cladding" },
	{ id = "00 9B", desc = "Wooden Flooring" },
	{ id = "00 9C", desc = "Springtide Sprinkles" },
	{ id = "00 9D", desc = "Tintered Cladding" },
	{ id = "00 9E", desc = "Castle Cladding" },
	{ id = "00 A0", desc = "Blue Flagstone Flooring" },
	{ id = "00 A1", desc = "Red Carpeting" },
	{ id = "00 A2", desc = "Castle Tiling" },
	{ id = "00 A3", desc = "Crashes Computer" },
	{ id = "01 00012C", desc = "Block" },
	{ id = "01 2D", desc = "Oaken Club" },
	{ id = "01 2E", desc = "Copper Sword" },
	{ id = "01 30", desc = "Steel Broadsword" },
	{ id = "01 31", desc = "Aurora Blade" },
	{ id = "01 32", desc = "Fire Blade" },
	{ id = "01 33", desc = "Sword of Kings" },
	{ id = "01 34", desc = "Falcon Blade" },
	{ id = "01 35", desc = "Sword of Ruin" },
	{ id = "01 36", desc = "Cypress Stick" },
	{ id = "01 37", desc = "Stone Sword" },
	{ id = "01 38", desc = "Erdrick's Sword" },
	{ id = "01 39", desc = "Copper Sword" },
	{ id = "01 3A", desc = "Iron Broadsword" },
	{ id = "01 3B", desc = "Aurora Blade" },
	{ id = "01 3C", desc = "Giant Mallet" },
	{ id = "01 3D", desc = "Sledgehammer" },
	{ id = "01 3E", desc = "War Hammer" },
	{ id = "01 40", desc = "Hammer of the Builder" },
	{ id = "01 42", desc = "Giant Mallet" },
	{ id = "01 43", desc = "Sledgehammer" },
	{ id = "01 44", desc = "Hela's Hammer" },
	{ id = "01 45", desc = "War Hammer" },
	{ id = "01 46", desc = "Stone Axe" },
	{ id = "01 47", desc = "Iron Axe" },
	{ id = "01 48", desc = "Battleaxe" },
	{ id = "01 49", desc = "Readsman's Axe" },
	{ id = "01 4A", desc = "Axe of the Builder" },
	{ id = "01 4B", desc = "Not Used (Iron Axe)" },
	{ id = "01 51", desc = "Leather Shield" },
	{ id = "01 52", desc = "Iron Shield" },
	{ id = "01 53", desc = "Steel Shield" },
	{ id = "01 54", desc = "Magic Shield" },
	{ id = "01 55", desc = "Silver Shield" },
	{ id = "01 56", desc = "Hero's Shield" },
	{ id = "01 57", desc = "Thanatos Shield" },
	{ id = "01 5D", desc = "Wayfarer's Clothes" },
	{ id = "01 5E", desc = "Training Togs" },
	{ id = "01 60", desc = "Chain Mail" },
	{ id = "01 61", desc = "Flowing Dress" },
	{ id = "01 62", desc = "Plain Clothes" },
	{ id = "01 63", desc = "Ragged Rags" },
	{ id = "01 69", desc = "Leather Armour" },
	{ id = "01 6A", desc = "Iron Armour" },
	{ id = "01 6B", desc = "Full Plate Armour" },
	{ id = "01 6C", desc = "Spiked Armour" },
	{ id = "01 6D", desc = "Magic Armour" },
	{ id = "01 6E", desc = "Auroral Armour" },
	{ id = "01 70", desc = "Scandalous Swimsuit" },
	{ id = "01 76", desc = "Meteorite Bracer" },
	{ id = "01 77", desc = "Talaria" },
	{ id = "01 78", desc = "Gold Ring" },
	{ id = "01 79", desc = "Strength Ring" },
	{ id = "01 7A", desc = "Dragon Scale" },
	{ id = "01 7B", desc = "Ring of Criticality" },
	{ id = "01 7C", desc = "Ring of Immunity" },
	{ id = "01 7D", desc = "Full Moon Ring" },
	{ id = "01 7E", desc = "Ring of Awakening" },
	{ id = "01 80", desc = "Catholican Ring" },
	{ id = "01 81", desc = "Ruby of Protection" },
	{ id = "01 83", desc = "Gourmands Girdle" },
	{ id = "01 84", desc = "Steel Sabatons" },
	{ id = "01 85", desc = "Featherfall Footwear" },
	{ id = "01 8B", desc = "Shovel" },
	{ id = "01 94", desc = "Stone (Material)" },
	{ id = "01 95", desc = "Grassy Leaves (Material)" },
	{ id = "01 96", desc = "White Petals (Material)" },
	{ id = "01 97", desc = "Wood (Material)" },
	{ id = "01 9A", desc = "Cord (Material)" },
	{ id = "01 9E", desc = "Water (Material)" },
	{ id = "01 A1", desc = "Flour (Material)" },
	{ id = "01 A3", desc = "Cactus Cutlet (Material)" },
	{ id = "01 A6", desc = "Glass (Material)" },
	{ id = "01 A7", desc = "Spring (Material)" },
	{ id = "01 A9", desc = "Coal (Material)" },
	{ id = "01 AA", desc = "Copper (Material)" },
	{ id = "01 AB", desc = "Iron (Material)" },
	{ id = "01 B0", desc = "Iron Ingot (Material)" },
	{ id = "01 B1", desc = "Steel Ingot (Material)" },
	{ id = "01 B2", desc = "Block" },
	{ id = "01 B3", desc = "Not Used (Black Square)" },
	{ id = "01 B4", desc = "Not Used (Root of some kind)" },
	{ id = "01 B6", desc = "Branch (Material)" },
	{ id = "01 BE", desc = "Super Cement (Material)" },
	{ id = "01 CA", desc = "Prickly Peach (HP +10)" },
	{ id = "01 CD", desc = "Gold (Material)" },
	{ id = "01 CE", desc = "Dowelling (Material)" },
	{ id = "01 D2", desc = "Pink Petals (Material)" },
	{ id = "01 D3", desc = "Yellow Petals (Material)" },
	{ id = "01 D5", desc = "Blue Tablet Fragments (Material)" },
	{ id = "01 D6", desc = "Strong Stalks (Material)" },
	{ id = "01 DB", desc = "Cotton (Material)" },
	{ id = "01 E1", desc = "Pure Water (Material)" },
	{ id = "01 E4", desc = "Fibrous Fond (Material)" },
	{ id = "01 E5", desc = "Fibrous Fabric (Material)" },
	{ id = "01 F1", desc = "Shot Silk (Material)" },
	{ id = "01 F4", desc = "Sulphur (Material)" },
	{ id = "01 F9", desc = "Volcovoltalc Magimotor (Material)" },
	{ id = "01 FA", desc = "Thermobattery (Material)" },
	{ id = "01 FB", desc = "Orichalcum (Material)" },
	{ id = "01 FE", desc = "Holyhock Seed" },
	{ id = "02 C7", desc = "Chimera Feather (Material)" },
	{ id = "02 E8", desc = "Monster Egg" },
	{ id = "03 E9", desc = "Dirt Block" },
	{ id = "04 0C", desc = "Stone Wall" },
	{ id = "04 14", desc = "Flagstone Block" },
	{ id = "04 30", desc = "Golemite" },
	{ id = "04 C6", desc = "Tainted Tree" },
	{ id = "04 C7", desc = "Twisted Tree Trunk" },
	{ id = "04 A9", desc = "Signpost" },
	{ id = "04 B4", desc = "Collary Flower" },
	{ id = "04 E6", desc = "Pot" },
	{ id = "04 E7", desc = "Leather Sack" },
	{ id = "04 ED", desc = "Stone Steps" },
	{ id = "04 F5", desc = "Wooden Door" },
	{ id = "04 FE", desc = "Torch" },
	{ id = "05 00", desc = "Bonfire" },
	{ id = "05 01", desc = "Cookfire" },
	{ id = "05 02", desc = "Lanten" },
	{ id = "05 03", desc = "Candlestick" },
	{ id = "05 04", desc = "Candalabrum" },
	{ id = "05 05", desc = "Brazier" },
	{ id = "05 06", desc = "Pot Plant" },
	{ id = "05 07", desc = "Bottles" },
	{ id = "05 08", desc = "Crockery" },
	{ id = "05 09", desc = "Dressing Table" },
	{ id = "05 0A", desc = "Small Table" },
	{ id = "05 0B", desc = "Big Table" },
	{ id = "05 0C", desc = "Stone Table" },
	{ id = "05 0D", desc = "Dining Table" },
	{ id = "05 0E", desc = "Wooden Stool" },
	{ id = "05 10", desc = "Throne" },
	{ id = "05 11", desc = "Straw Mattress" },
	{ id = "05 12", desc = "Simple Bed" },
	{ id = "05 13", desc = "King-sized Bed" },
	{ id = "05 14", desc = "Towel Rail" },
	{ id = "05 15", desc = "Bookcase" },
	{ id = "05 16", desc = "Chest of Drawers" },
	{ id = "05 17", desc = "Warddrobe" },
	{ id = "05 18", desc = "Grandfather Clock" },
	{ id = "05 19", desc = "Ornamental Swords" },
	{ id = "05 1A", desc = "Ornamental Armour" },
	{ id = "05 1B", desc = "Stove" },
	{ id = "05 1C", desc = "Sink" },
	{ id = "05 1D", desc = "Bathtub" },
	{ id = "05 1E", desc = "Goddess Statue" },
	{ id = "05 20", desc = "Item Display Stand" },
	{ id = "05 21", desc = "Treasure Chest" },
	{ id = "05 37", desc = "Spike" },
	{ id = "05 34", desc = "Infernal Statue" },
	{ id = "05 43", desc = "Stone Workbench" },
	{ id = "05 41", desc = "Tree Stump" },
	{ id = "05 44", desc = "Carpenter's Workstation" },
	{ id = "05 45", desc = "Builder's Workbench" },
	{ id = "05 46", desc = "Colour Wheel" },
	{ id = "05 47", desc = "Stone Forge" },
	{ id = "05 48", desc = "Hela's Hammer Sign" },
	{ id = "05 49", desc = "Brick Barbecue" },
	{ id = "05 4A", desc = "Miner's Refiner" },
	{ id = "05 4B", desc = "Sewing Station" },
	{ id = "05 4C", desc = "Herbalist's Cauldron" },
	{ id = "05 4D", desc = "Machinest's Workbench" },
	{ id = "05 4E", desc = "Welder's Workbench" },
	{ id = "05 50", desc = "Forbidden Altar" },
	{ id = "05 51", desc = "Furnace" },
	{ id = "05 52", desc = "Book" },
	{ id = "05 53", desc = "Note" },
	{ id = "05 54", desc = "Bashmobile" },
	{ id = "05 55", desc = "Cantlin Shield" },
	{ id = "05 56", desc = "Holyhock Seedling" },
	{ id = "05 57", desc = "Erdrick's Emblem (Crash's Game Outside Freebuild)" },
	{ id = "05 58", desc = "Infernal Ivy" },
	{ id = "05 59", desc = "Cannon" },
	{ id = "05 5A", desc = "Piston" },
	{ id = "05 5B", desc = "Pressure Plate" },
	{ id = "05 5C", desc = "Button" },
	{ id = "05 5D", desc = "Balista" },
	{ id = "05 5E", desc = "Magic Canon" },
	{ id = "05 63", desc = "Collosal Coffer" },
	{ id = "05 80", desc = "Chest" },
	{ id = "05 78", desc = "Blue Teleportal" },
	{ id = "05 79", desc = "Red Teleportal" },
	{ id = "05 95", desc = "Comfy Sofa" },
	{ id = "06 1B", desc = "Pippa's Blueprint" }
}
return t
