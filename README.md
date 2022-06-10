
# InkRLZ

Quick tool I made for some stream tech to allow me to draw on the screen.

I made it for my setup & configuration, and probably won't work for yours without tweaking.

It doesn't have a tray icon, so the easiest way to stop it is to run it through Visual Studio, but you can also kill the process.

Enter and exit ink mode with Ctrl+Alt+V, as a global keybinding.

When in ink mode, draw on the screen by clicking and dragging. Change the brush size with [ and ] keys, and 
use 1/2/3/4/5/6/7/8/9 to change colors. Use Z to undo, X to redo, and C to clear everything.

By default, there are only three colors; feel free to add your own by adding more to the `s_penColors` array.

For my setup, I only wanted the application to show up on one of my two monitors, which just so happened to be the behavior that happened when I coded it up the naive way. Multiple monitor support might take a bit of coding.