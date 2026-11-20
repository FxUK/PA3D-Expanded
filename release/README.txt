PA3D Expanded 1.9.132 - a 3D isometric view for Prison Architect
by FxUK - free fan mod, MIT licence (see PA3D Expanded\LICENSE.txt)
Made with AI assistance: much of the code, the model generators and these notes were written with an AI coding
assistant (Claude, by Anthropic), directed, tested and reviewed in game by FxUK.

Turns the game's hidden 3D mode into a full isometric 3D view: a free-turning camera, cutaway or lowered walls,
cartoon shading, outlines, textured 3D models for furniture, food and people, sitting, shadows and lamp light.

PLEASE NOTE
PA3D Expanded is released unfinished, as it is. I am not committed to developing it further - updates will only come if
time permits. You are welcome to improve, develop and expand it yourself; the MIT licence allows it, as long as
the credit to FxUK is kept.

WHAT YOU NEED
Prison Architect 1.02 (r11056), the 64-bit Steam version. On any other version the mod switches itself off and the
game runs as normal. Some models and the kitchen food come from the DLC; without the DLC those items simply do not
appear in your prison.

INSTALL
1. Close the game.
2. Back up your game folder's "data" folder if you have other mods in it.
3. Open the zip and copy everything in it into your Prison Architect folder (the one with Prison Architect64.exe),
   replacing files when asked.
4. Start the game and load a prison.

The zip adds:
  glu32.dll            the mod (it sits next to the game; Windows' own glu32.dll is not touched)
  PA3D Expanded\              settings, textures, food and head models, the log
  data\shapes\         the 3D models
  data\materials.txt   one change to the game's object list
  data\tilesetpadded.png  the repainted floors

KEYS
  F10   mod menu (every setting)
  Q / E turn the camera     middle mouse: turn freely
  F8    walls: up / cutaway / down
  F9    mod on / off

UNINSTALL
Delete glu32.dll, the PA3D Expanded folder, data\shapes, data\materials.txt and data\tilesetpadded.png from your game
folder, then in Steam: Prison Architect > Properties > Installed Files > Verify integrity of game files.

ANTIVIRUS
glu32.dll is unsigned and changes how the game draws while it runs, so some antivirus programs and Windows
SmartScreen may warn about it. It reads and writes only inside your game folder and does not use the internet.

PROBLEMS
Everything the mod does is written to PA3D Expanded\PA3D Expanded.log - include it if you report a problem.
If the frame rate is low, turn off Supersampling in the game's graphics options.
Settings are in PA3D Expanded\PA3D Expanded.ini (or the F10 menu); delete the file to go back to the defaults.

Credits: PA3D Expanded by FxUK. Menu by Dear ImGui (MIT, see PA3D Expanded\THIRD_PARTY_NOTICES.txt).
Prison Architect belongs to Introversion Software and Paradox Interactive. Not affiliated with or endorsed by them.
