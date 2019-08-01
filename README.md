# OOR Buster
Fixes out of range error and black screen when overclocking BenQ 144Hz monitors

## Details
Restores your current OSD language (fixes Out of Range error), picture mode (fixes black screen) and AMA setting after hitting over 144Hz on your BenQ 144Hz monitor. The OSD language and picture mode is cached upon launching the app, so if you switched your picture mode (e.g. from Standart to FPS1) and wish to use it, before switching to overclocked refresh rate right click the OOR Buster icon in tray and hit _Reload_.

## Launch parameters
4 parameters going in sequence (create a shortcut to `OorBuster.exe` and add these in _Target_ field):
```
<OOR delay> <Pic delay> <AMA delay> <Wake delay>
```
- OOR delay - after something changes with the screen (refresh rate, fullscreen exclusive, etc.), wait this long before removing Out of Range notification
- Pic delay - after OOR removal, wait this long before fixing black screen (not all monitors get black screen)
- AMA delay - after fixing black screen, wait this long before setting back AMA (this may be redundant for your monitor, but can't hurt)
- Wake delay - after screen wakes up (after screen sleep or system sleep), wait this long before starting the usual sequence

Default:
```
2000 500 250 3000
```
  
### To launch with Windows:
Put the .exe (or shortcut if using custom launch parameters) to 
```
%ProgramData%\Microsoft\Windows\Start Menu\Programs\StartUp
```

## Credits
Blur Busters: https://blurbusters.com

### Forum thread:
https://forums.blurbusters.com/viewtopic.php?f=8&t=5544
