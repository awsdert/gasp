# gasp
Game Assistive tech for Solo Play

## About
Got tired of waiting for PINCE to be usable so started this myself.
At the moment I can hook existing processes with this but have yet to
successfully change memory. Will refocus oon that once I crush a hidden
bug (haven't found the source, code given is 139 which according to a
result on google is possibly a segfault, yet to encounter that segfault
with a debugger though)

## APIs
Uses C under the hood but will be mostly scripted in Lua for so users
can customize for themselves (also make Trainers easier to design).
For now only plugged in the moon libraries however I plan to eventually
plug in OpenAL related libraries and libraries for those who are both
deaf & blind, however the use of such libraries will be have to be
impleneted by the users themselves because I don't have those kinda
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
* https://github.com/Immediate-Mode-UI/Nuklear
