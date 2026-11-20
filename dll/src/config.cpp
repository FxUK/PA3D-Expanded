#include "config.h"
#include <cstdio>
#include <cstdlib>

static const char* S = "PA3D Expanded";

static float rdF(const std::string& p, const char* k, float d) {
    char def[64], buf[64]; snprintf(def, sizeof def, "%g", d);
    GetPrivateProfileStringA(S, k, def, buf, sizeof buf, p.c_str());
    return (float)atof(buf);
}
static int  rdI(const std::string& p, const char* k, int d) { return (int)GetPrivateProfileIntA(S, k, d, p.c_str()); }
static void wrF(const std::string& p, const char* k, float v) { char b[64]; snprintf(b, sizeof b, "%g", v); WritePrivateProfileStringA(S, k, b, p.c_str()); }
static void wrI(const std::string& p, const char* k, int v)   { char b[64]; snprintf(b, sizeof b, "%d", v); WritePrivateProfileStringA(S, k, b, p.c_str()); }

#define F(name) name = rdF(p, #name, name)
#define I(name) name = rdI(p, #name, name)
#define B(name) name = rdI(p, #name, name ? 1 : 0) != 0

void IsoConfig::Load(const std::string& p) {
    B(isoEnabled); B(force3D); F(pitchDeg); F(startYawDeg); F(rotStepDeg); F(rotSpeed); F(zoom);
    B(flipY); F(upSign); B(rotateScroll); B(perspective); F(panSpeed); B(snapLying); F(snapMax); B(extraModels); B(resolveBank2); B(ownFloor); B(liftDoors); B(snapToWalls); B(hideSunShadows); B(hideCutFittings); B(isoOnMenu); B(redrawMissingWalls); B(pickFix); B(deferOverlays); B(dropBadFrames); B(fixExtraPass); B(holdExtraPassFrame); B(wallButton); B(pokeFittings); B(cullOffscreen); F(bedTop); B(floorFromTileset); B(wallArt); B(showItems); B(uniformTints); B(prisonerVariety); B(prisonerHeads); B(diagLog); B(diagLines); B(mvCache); B(splitDoubleDoors); B(cellLightsOff); B(showEquipment); B(nakedSkin); B(items3D); B(headSwap); B(glErrTrace); B(poseDiag); B(liftOnFurniture); B(rotateBoiler); B(showHands); B(personShadows); B(bulbPools); B(spriteFallback); B(hideUtilities); B(earlyCull); F(tinyCullPx); F(nightDark); F(outline); F(silhouette); F(seatDrop); B(ingredientCrates); B(foodModels); B(objectShadows); B(bodyScale); F(shopFrontOut); B(variantDiag); F(itemMaxSize); B(liveSpriteIds); F(tileSoft); F(tileFlat); F(bedShift); F(cullMargin); F(fittingPoke); F(wallBtnRight); F(wallBtnTop); B(deferArrays); B(cutDoors); F(doorCutScale); F(doorShift); B(wallAutoGround); B(billboardBubbles); I(bubbleTexA); I(bubbleTexB); F(bubbleDrop); F(bubbleLift); F(bubbleScale); F(fovDeg); F(minPitchDeg);
    I(wallMode); B(roomCut); I(cutStyle); I(focusMode); F(cutRadius); F(wallHeight); F(wallLift); F(wallThick); B(doorAutoDepth); B(doorFixes); F(stub); F(downStub); F(cutScreen); F(grit); F(floorBright); F(warm); F(style);
    B(cartoon); F(toon); F(sat); F(wallTint[0]); F(wallTint[1]); F(wallTint[2]);
    I(keyRotL); I(keyRotR); I(keyMenu); I(keyToggle); I(keyWalls); B(consumeKeys); B(verboseLog); I(hideTexA); I(hideTexB);
    B(wallBatch);
}
#undef F
#undef I
#undef B
#define F(name) wrF(p, #name, name)
#define I(name) wrI(p, #name, name)
#define B(name) wrI(p, #name, name ? 1 : 0)
void IsoConfig::Save(const std::string& p) const {
    B(isoEnabled); B(force3D); F(pitchDeg); F(startYawDeg); F(rotStepDeg); F(rotSpeed); F(zoom);
    B(flipY); F(upSign); B(rotateScroll); B(perspective); F(panSpeed); B(snapLying); F(snapMax); B(extraModels); B(resolveBank2); B(ownFloor); B(liftDoors); B(snapToWalls); B(hideSunShadows); B(hideCutFittings); B(isoOnMenu); B(redrawMissingWalls); B(pickFix); B(deferOverlays); B(dropBadFrames); B(fixExtraPass); B(holdExtraPassFrame); B(wallButton); B(pokeFittings); B(cullOffscreen); F(bedTop); B(floorFromTileset); B(wallArt); B(showItems); B(uniformTints); B(prisonerVariety); B(prisonerHeads); B(diagLog); B(diagLines); B(mvCache); B(splitDoubleDoors); B(cellLightsOff); B(showEquipment); B(nakedSkin); B(items3D); B(headSwap); B(glErrTrace); B(poseDiag); B(liftOnFurniture); B(rotateBoiler); B(showHands); B(personShadows); B(bulbPools); B(spriteFallback); B(hideUtilities); B(earlyCull); F(tinyCullPx); F(nightDark); F(outline); F(silhouette); F(seatDrop); B(ingredientCrates); B(foodModels); B(objectShadows); B(bodyScale); F(shopFrontOut); B(variantDiag); F(itemMaxSize); B(liveSpriteIds); F(tileSoft); F(tileFlat); F(bedShift); F(cullMargin); F(fittingPoke); F(wallBtnRight); F(wallBtnTop); B(deferArrays); B(cutDoors); F(doorCutScale); F(doorShift); B(wallAutoGround); B(billboardBubbles); I(bubbleTexA); I(bubbleTexB); F(bubbleDrop); F(bubbleLift); F(bubbleScale); F(fovDeg); F(minPitchDeg);
    I(wallMode); B(roomCut); I(cutStyle); I(focusMode); F(cutRadius); F(wallHeight); F(wallLift); F(wallThick); B(doorAutoDepth); B(doorFixes); F(stub); F(downStub); F(cutScreen); F(grit); F(floorBright); F(warm); F(style);
    B(cartoon); F(toon); F(sat); F(wallTint[0]); F(wallTint[1]); F(wallTint[2]);
    I(keyRotL); I(keyRotR); I(keyMenu); I(keyToggle); I(keyWalls); B(consumeKeys); B(verboseLog); I(hideTexA); I(hideTexB);
    B(wallBatch);
}
