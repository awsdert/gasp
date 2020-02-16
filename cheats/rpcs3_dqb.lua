t = {
	app = { name = "rpcs3", desc = "RPCS3 - Playstation 3 Emulator" },
	emu = { desc = "Dragon Quest Builders" },
	endian = "Big",
	list = {
{ desc = "Days", addr = 18036876676, signed = 2 },
{ desc = "HP Upto", addr = 18036876678, signed = 2 },
{ desc = "HP Left", addr = 18036876680, signed = 2 },
{ desc = "Hunger Upto", addr = 18036876682, signed = 2 },
{ desc = "Hunger Left", addr = 18036876684, signed = 2 },
{ desc = "Attack", addr = 18036876686, signed = 2 },
{ desc = "Defence", addr = 18036876688, signed = 2 },
{ desc = "???", addr = 18036876692, signed = 2 },
-- Once implemented will render the below list wasteful
{ desc = "Items", addr = 18036976864,
	count = 15, size = 4, split = {
		{desc="ID",bytes = 2}, {desc="Qty",signed = 2}
	}
},
-- Imported from GameConqueror cheat file
{ desc = "Slot 1 ID", addr = 18036976864, bytes = 2 },
{ desc = "Slot 1", addr = 18036976867, signed = 1 },
{ desc = "Slot 2 ID", addr = 18036976868, bytes = 2 },
{ desc = "Slot 2", addr = 18036976871, signed = 1 },
{ desc = "Slot 3 ID", addr = 18036976872, bytes = 2 },
{ desc = "Slot 3", addr = 18036976875, signed = 1 },
{ desc = "Slot 4 ID", addr = 18036976876, bytes = 2 },
{ desc = "Slot 4", addr = 18036976879, signed = 1 },
{ desc = "Slot 5", addr = 18036976883, signed = 1 },
{ desc = "Slot 6", addr = 18036976887, signed = 1 },
{ desc = "Slot 7", addr = 18036976891, signed = 1 },
{ desc = "Slot 8", addr = 18036976895, signed = 1 },
{ desc = "Slot 9", addr = 18036976899, signed = 1 },
{ desc = "Slot 10", addr = 18036976903, signed = 1 },
{ desc = "Slot 11", addr = 18036976907, signed = 1 },
{ desc = "Slot 12", addr = 18036976911, signed = 1 },
{ desc = "Slot 13", addr = 18036976915, signed = 1 },
{ desc = "Slot 14", addr = 18036976919, signed = 1 },
{ desc = "Slot 15", addr = 18036976923, signed = 1 },
-- Once implemented will render the below list wasteful
{ desc = "Gear", addr = 18036976924,
	count = 16, size = 4, split = {
		{desc="ID",bytes = 2}, {desc="HP",signed = 2}
	}
},
{ desc = "Gear 4 ID", addr = 18036976936, signed = 2 },
{ desc = "Gear 4", addr = 18036976938, signed = 2 },
{ desc = "Gear 5", addr = 18036976942, signed = 2 },
{ desc = "Gear 6", addr = 18036976946, signed = 2 }
	}
}
return t
