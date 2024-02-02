# NPP Q3 PMove

## Update

~~Epic has discontinued NPP in favour of a new TBA prediction plugin. Archiving this repo, incase anyone wants to stare at it for weird history reasons.~~

Seems like NPP is in active usage on Epic's new Mover plugin, so probably this repo might come in handy for dissecting NPP now! Probably I will move this Quake implementation over to Mover when it's more production ready.

## Summary

Hi! Here's a shitty port of Quake 3's Player Movement onto Epic's Network Prediction Plugin.
It was originally intended to be used for Mesa, but now it's sitting here doing nothing; do whatever you want with it, I don't give a fuck.
Hopefully, it serves as a jumping off point for someone that hates CMC as much as I do. Won't be much work to perfectly emulate Quake 3 / Source exactly from here.

## Some notes

- This project was originally on some UE5-main build, -careful what you copy since stuff might have been upgraded since.
- Straight rip from early Mesa while I was testing with movement, - there might be some junk.
- I don't plan to keep this updated, if you're brave enough to fuck around with NPP, upgrade it yourself.
- No, there isn't any blueprint support, go away.

- Not intended to be an exact 1:1 port of PMove, - It's a minimal port.
- Fixed tick is disabled in NPP in project settings, because of funky behaviour (probably skill issue on my part), - I use semi-fixed tick.
- Look in UMesaGameData and Content/MesaGameData for input mapping.
