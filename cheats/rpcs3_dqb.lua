t = {
	app = { name = "rpcs3", desc = "RPCS3 - Playstation 3 Emulator" },
	emu = { desc = "Dragon Quest Builders" },
	endian = "Big",
	list = {
{ desc = "Player", addr = 18036876676,
	list = { 
		{ desc = "Days", offset = 0, signed = 2 },
		{ desc = "HP Upto", offset = 2, signed = 2 },
		{ desc = "HP Left", offset = 4, signed = 2 },
		{ desc = "Hunger Upto", offset = 6, signed = 2 },
		{ desc = "Hunger Left", offset = 8, signed = 2 },
		{ desc = "Attack", offset = 10, signed = 2 },
		{ desc = "Defence", offset = 12, signed = 2 }
	}
},
{ desc = "???", addr = 18036876692, signed = 2 },
{ desc = "Items", addr = 18036976864,
	count = 15, size = 4, split = {
		{desc="ID",bytes = 2}, {desc="Qty",signed = 2} } },
{ desc = "Gear", addr = 18036976924,
	count = 16, size = 4, split = {
		{desc="ID",bytes = 2}, {desc="HP",signed = 2} } }
	}
}
return t
