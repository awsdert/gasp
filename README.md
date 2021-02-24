# gasp
Game Assistive tech for Solo Play

## About
Got tired of waiting for PINCE to be usable so started this myself.
Gasp currently can edit known memory but scanning it is gonna be
inefficeint until I've re-worked what the lua wrapper for aobscan
returns and also implemented integer search, string search will work
in aobscan but again I need to get round to making that more efficeint.
The basic goal is to be a drop-in replacement for GameConqueror with
more flexability, there is no intent to make this a full debugging
interface like PINCE seems to be steering towards, scanmem is not used
neither is gdb, the API's supported by manjaro cinnamon minimal is what
I'm working with so don't expect me to target anything more than that
(although I will plug-in some #ifdef code for _WIN32 for sake of
ReactOS builds)

## Current target
Currently planning to rewrite from scratch using cleaner code and the
now functional alu project (https://github.com/awsdert/alu), I will
finish fixing some minor bugs and try out an idea I have for retreiving
the remainder of a floating division, the division has a rounding error
but the much needed bit comparison function (alu__cmp_int2int) is working
as expected, it will be needed for replicating the bit search feature
that Cheat Engine has (yes I'm using that as the inspiration for features
in this project). Anyways this little section was added because I realised
I hadn't uploaded for a long time and peops might've thought the project
was dead, no it isn't, I just got severly side tracked with other projects,
there's still things I can do in this project that I just won't get in
PINCE (which for time being is now the best option for linux), example of
which is cheat lists in full lua, makes converting existing cheats easier.

## APIs
Uses C under the hood but will be mostly scripted in Lua for so users
can customize for themselves (also make Trainers easier to design).
For now only plugged in the moon libraries however I plan to eventually
plug in OpenAL related libraries and libraries for those who are both
deaf & blind, however the use of such libraries will be have to be
implemented by the users themselves because I don't have those kinda
touch screens nor am I inclined to buy them.

This will not be compatible with Cheat Engine scripts, they will have
to be ported to fit the libraries I plug in, won't stop anyone from
branching when I stabilize the API to make a port of Cheat Engine though

## 3rd Party
These are the open source libraries I'm currently using whether
directly or indrectly (not including system libraries)
* https://github.com/lua/lua
* https://github.com/stetre/moongl
* https://github.com/stetre/moonglfw
* https://github.com/stetre/moonnuklear
* https://github.com/vurtun/nuklear
