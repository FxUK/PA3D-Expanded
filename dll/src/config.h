#pragma once
#include <windows.h>
#include <string>

struct IsoConfig {
    // Camera
    bool  isoEnabled   = true;    // iso camera replaces the game's 3D camera
    bool  force3D      = true;    // switch the game's hidden 3D renderer on automatically
    float pitchDeg     = 35.264f; // viewing angle above the ground (true isometric)
    float startYawDeg  = 45.0f;
    float rotStepDeg   = 45.0f;
    float rotSpeed     = 12.0f;   // rotation animation speed (higher = snappier)
    float zoom         = 0.85f;   // ortho size multiplier vs game zoom
    bool  flipY        = false;   // mirror fix if the map looks flipped
    float upSign       = -1.0f;   // world "up" axis sign on Z (game camera sits at -Z)
    bool  rotateScroll = true;    // re-map WASD / drag panning into the rotated view
    bool  billboardBubbles = true; // speech / thought bubbles drawn upright instead of lying on the floor
    int   bubbleTexA = 5, bubbleTexB = 27;   // game texture ids: bubbles + icons, bubble text
    float bubbleDrop = 1.4f, bubbleLift = 1.0f, bubbleScale = 1.0f;
    bool  wallAutoGround = true;  // measure each wall piece and lower it so its base meets the floor (ignores the manual lift)
    bool  cutDoors     = false;    // doors are lowered with their wall
    float doorCutScale = 0.6f;
    float doorShift    = -1.0f;    // slide doors this far (tiles) towards the outer face of their room    // ...to this fraction of their height
    bool  deferArrays = false;       // also replay flat array draws on top: this blacked out floors, so it is off
    bool  floorFromTileset = true;
  bool  wallArt = true;
  bool  variantDiag = false;     // log register / memory dumps for tree and sink draws (diagnostic)
  float shopFrontOut = 0.12f;    // shop counters moved out of the wall line by this much (tiles), to overhang as in 2D; negative = the other way
  bool  glErrTrace = false;      // diagnostic: glGetError after each mod step, log GLERR (costs frame time)
  bool  showHands = true;       // round hands on every person (at the sides; together in front when cuffed), as in 2D
  bool  objectShadows = true;   // soft contact shadows under furniture and floor objects (footprint, fading rim)
  bool  personShadows = true;   // a soft round shadow on the floor under every person
  bool  bulbPools = true;       // every lamp casts its own pool of warm light (indoors: bright under each bulb, dimmer between; outdoors at night: pools on the dark ground)
  float outline = 0.55f;        // 1.9.108: 0..1 how dark the drawn edge lines on objects are (0 = off)
  float nightDark = 1.0f;       // 0..1 how dark the night gets outdoors and in unlit rooms (0 = the old fixed brightness)
  bool  rotateBoiler = true;    // water boilers turned 90 degrees: the tank runs along the wall
  bool  liftOnFurniture = true; // food trays / meals the game draws flat on the floor are lifted onto the table or sofa they lie on
  bool  poseDiag = false;        // diagnostic: PRISPOSE size / height of every prisoner draw (reads every vertex)
  bool  bodyScale = false;       // prisoners drawn at their own body scale (Bio BodyScale 0.7..1.24), as in 2D
  bool  headSwap = true;         // each prisoner's body wears the stock 3D head that matches their 2D head (bald, crest, lots of hair, short)
  bool  items3D = false;         // loose items drawn as the mod's simple coloured 3D boxes (1.9.90: off - they looked bad; the game's own stack models and the extruded art show instead)
  bool  nakedSkin = true;        // naked prisoners (shower, changing) show bare skin instead of the uniform, as in 2D
  bool  showEquipment = true;    // cuffs, batons, tasers, guns (2D overlay sprites) drawn as small 3D items
  bool  cellLightsOff = true;    // cell rooms go dark during the Sleep regime (visual only)
  bool  splitDoubleDoors = true; // double doors open by parting their two leaves (the game slides the whole door sideways)
  bool  wallBatch = true;        // 1.9.130: walls recorded once and drawn as one batch (rebuilt when they change); off = the game's piece-by-piece pass
  bool  foodModels = true;      // 1.9.128: kitchen food (DLC crates, pots, pans, trays, bowls, items) drawn with the mod's own 3D models (PA3aiD\food3d)
  bool  ingredientCrates = true; // 1.9.126: boxes of raw ingredients on the floor use the stock crate model (ingredient1.obj), not the flat sprite
  float seatDrop = 0.2f;       // 1.9.125: how far (tiles) a person sitting still on a sofa / bench seat is lowered (0 = off)
  bool  mvCache = true;         // 1.9.125: modelview read back only after a matrix call (off = read on every draw, as before)
  float silhouette = 0.6f;      // 1.9.125: 0..1 how dark the outline drawn round objects' and people's outer edges is (0 = off)
  bool  diagLines = false;       // 1.9.122: write the tagged diagnostic lines (EQUIPQ, DOORSHIFT, PRISSTAT...) to the log; off = status lines only
  bool  diagLog = true;          // one-off diagnostic dumps in the log (model draws, cells, FBOs, Bio...); off = status lines only
  bool  prisonerHeads = false;   // the prisoner's own 2D head sprite painted onto the 3D head (looked odd in game: off; skin and hair colours stay)
  bool  prisonerVariety = true;  // prisoners get their own skin colour (Bio SkinColour) and hair colour (head sprite) in 3D
  bool  uniformTints = true;     // prisoner uniforms take their security-level colour from the game's 2D sprite tint
  bool  showItems = false;        // draw loose items (crates, pots, trays) that have no 3D model, from the game's 2D sprites
  float itemMaxSize = 1.6f;      // sprites smaller than this (tiles) count as items
  bool  earlyCull = true;        // off-screen objects skipped before the game draws them (only once the position check has passed)
  bool  hideUtilities = true;    // pipes / cables / valves (under the floor) are not drawn
  float tinyCullPx = 7.0f;       // small props are skipped when a tile is narrower than this many pixels (0 = never)
  bool  spriteFallback = true;   // every small hidden 2D sprite with no 3D item is drawn as a thin extrusion of its art (nothing invisible)
  bool  liveSpriteIds = true;     // look sprite ids up in the game's own name map (fixes objects drawn with another object's model)           // walls take their colour from their own 2D art   // 3D floor drawn from the game's own 2D floor art
  float tileSoft = 1.2f;         // softening of that art (mip bias): higher = softer
  float tileFlat = 0.2f;         // 0..1 pull of each tile towards its average colour (flatter, low-poly look)
  float bedTop = 0.47f;
  float bedShift = 0.75f;         // sleepers slide this far (tiles) towards their feet so their head lands on the pillow          // height people lying on a bed are brought down to (0 = leave the game's height)
  bool  cullOffscreen = true;     // skip 3D draws that land well off screen (big prisons)
  float cullMargin = 1.5f;       // tiles of margin around the screen edge
  bool  pokeFittings = true;      // wall fittings stand just proud of the thicker 3D walls
  float fittingPoke = 0.03f;     // how far proud, in tiles
  bool  wallButton = true;        // on-screen Walls button left of the clock
  float wallBtnRight = 260.0f;   // its distance from the right edge of the window, pixels
  float wallBtnTop = 8.0f;       // its distance from the top
  bool  holdExtraPassFrame = true;  // do not show the frame in which the game makes its time-lapse picture (the periodic flicker)
  bool  fixExtraPass = true;      // restore the 3D camera after the game's extra frame-buffer pass (the periodic flicker)
  bool  dropBadFrames = false;     // a frame that drew only a small fraction of the usual objects/walls is not shown (the periodic flicker)
  bool  deferOverlays = true;      // alert beacons and similar flat overlays are drawn on top of the 3D scene
    bool  pickFix = true;            // mouse picking uses our camera matrices
    bool  redrawMissingWalls = true; // draw a block for wall cells the game builds no 3D piece for (a single wall between two doors)
    bool  isoOnMenu    = false;    // the main menu fly-over also uses the iso camera
    bool  hideCutFittings = true; // wall fittings (windows, lights, paintings, signs) vanish with their cut wall
    bool  hideSunShadows = true;  // hide the game's flat 2D sun-shadow layer (it does not match the 3D objects)
    bool  snapToWalls  = true;    // slide wall-mounted objects (toilet, bookcase, TV) against the thinner wall
    bool  doorFixes    = true;     // doors: thickness with the walls, fit to the opening, cut with the walls, double doors part, outer-face shift
    bool  liftDoors    = false;    // lower the game's door models by the wall lift so they stand on the floor
    bool  ownFloor     = true;    // PA3aiD draws its own procedural floor over the game's terrain
    bool  resolveBank2 = true;    // resolve names from the 2nd extra sprite bank (benches, gates...): experimental
    bool  extraModels  = true;    // models for sprites past the 871 base-game slots (gates, benches, floodlights...)
    bool  snapLying    = true;    // lower people lying down so they touch the floor
    float snapMax      = 0.65f;   // ...but only if they hover less than this (tiles): floor sleepers hover about 0.5, a person on a bed about 0.76+ and stays put
    float panSpeed     = 1.7f;    // multiplier on WASD / key panning
    bool  perspective  = true;    // true perspective camera (false = flat isometric)
    float fovDeg       = 26.0f;   // vertical field of view in perspective mode
    float minPitchDeg  = 30.0f;   // lowest tilt allowed when free-rotating (keeps the sky out of view)

    // Walls
    int   wallMode     = 1;       // 0 up, 1 cutaway, 2 down
    bool  roomCut      = true;    // Sims-style cutaway (walls with rooms behind them)
    int   cutStyle     = 0;       // 0 shrink (Sims), 1 dither fade
    int   focusMode    = 0;       // 0 mouse, 1 screen centre
    float cutRadius    = 0.0f;    // tiles, 0 = unlimited
    float wallHeight   = 2.0f;    // world units, calibrate if stubs look wrong
    float wallThick    = 0.75f;    // wall thickness in tiles (the game builds 0.5; 1.0 = full tile)
    bool  doorAutoDepth = true;    // doors get thicker / thinner with wallThick (same proportion as to the game's 0.5 wall)
    float wallLift     = 0.0f;    // moves 3D walls along model Y (fixes walls floating above the floor)
    float stub         = 0.5f;    // fraction of height kept for cut walls in cutaway (half height, user 8 Nov)
    float cutScreen    = 2.0f;    // (1.9.92: Cutaway All is the default view) cutaway by screen: walls in the screen below this clip-space y are cut (0 = lower half: Close, 0.5 = lower 75 %: Far; -2 = old cursor / room cut)
    float downStub     = 0.15f;   // fraction of height kept for every wall in Walls Down
    float grit         = 0.35f;
    float warm         = 0.3f;       // warm (orange), softer lighting on floors, walls and objects
    float floorBright  = 1.0f;      // brightness multiplier for near-white floor tiles (1 = unchanged)
    float style        = 0.6f;    // how much object textures are flattened to painted colour
    bool  cartoon      = true;
    float toon         = 0.8f;    // cel-band strength
    float sat          = 1.3f;    // saturation
    float wallTint[3]  = {1.0f, 1.0f, 1.0f};

    // Keys (virtual-key codes)
    int  keyRotL   = 'Q';
    int  keyRotR   = 'E';
    int  keyMenu   = VK_F10;
    int  keyToggle = VK_F9;
    int  keyWalls  = VK_F8;
    bool consumeKeys = false;     // stop the game also seeing Q/E

    int  hideTexA = 3, hideTexB = 0;   // 2D layers (by texture id) hidden while the 3D view is on
    bool verboseLog = true;

    void Load(const std::string& path);
    void Save(const std::string& path) const;
};
