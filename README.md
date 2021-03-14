# gasp
Game Assistive tech for Solo Play

## About
Got tired of waiting for PINCE to be usable so started this myself.

## Current target
Started the re-write I planned, 

## APIs
Uses C under the hood but will be mostly scripted in Lua so users
can customize for themselves (also make Trainers easier to design).
Found a better GUI framework called tekUI, for manjaro I had to add
"-j 1" after the make command in the build file but otherwise is
easy to compile

This will not be compatible with Cheat Engine scripts, lua is used
for the sake of easing the port process, I will consider serious
support for windows after I'm satisfied with linux support, you're
free to branch this to support it prorerly yourself though, I wll
advise you to wait until I'm satisfied gasp can properly scan and
communicate ongoing scan results to the GUI though.

## 3rd Party
These are the open source libraries I'm currently using whether
directly or indrectly (not including system libraries)
* https://github.com/lua/lua
* http://tekui.neoscientists.org/

Gasp used to use these but I changed to tekUI for the sake of
easing accessibility support, if I'm right then I don't have to
do anything more to enable screenreaders and the like to
understand the GUI elements I'll be working with for devlopement,
please let me know if you experience problems with the results
* https://github.com/stetre/moongl
* https://github.com/stetre/moonglfw
* https://github.com/stetre/moonnuklear
* https://github.com/vurtun/nuklear
