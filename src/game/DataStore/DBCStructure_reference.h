#pragma once

struct AnimationDataEntry
{
    uint32  ID;
    char*   Name;
    uint32  Weaponflags;
    uint32  Bodyflags;
    int32   Field_0_7_0_3694_004;
    uint32  Flags;
    uint32  Fallback;
};

struct AreaPOIEntry
{
    uint32  ID;
    int32   Importance;
    int32   Icon;
    int32   FactionID;
    float   Pos[3];
    int32   ContinentID;
    uint32  Flags;
    int32   AreaID;
    char*   Name_lang[8];

    char*   Description_lang[8];

    int32   WorldStateID;
};

struct AttackAnimKitsEntry
{
    uint32  ID;
    int32   ItemSubclassID;
    int32   AnimTypeID;
    int32   AnimFrequency;
    int32   WhichHand;
};

struct AttackAnimTypesEntry
{
    uint32  AnimID;
    char*   AnimName;
};

struct CameraShakesEntry
{
    uint32  ID;
    int32   ShakeType;
    uint32  Direction;
    float   Amplitude;
    float   Frequency;
    float   Duration;
    float   Phase;
    float   Coefficient;
};

struct Cfg_CategoriesEntry
{
    uint32  ID;
    uint32  Region;
    char*   Name_lang[8];

};

struct Cfg_ConfigsEntry
{
    uint32  ID;
    uint32  RealmType;
    uint32  PlayerKillingAllowed;
    uint32  Roleplaying;
};

struct CharBaseInfoEntry
{
    uint8   RaceID;
    uint8   ClassID;
};

struct CharHairGeosetsEntry
{
    uint32  ID;
    uint32  RaceID;
    uint32  SexID;
    uint32  VariationID;
    uint32  GeosetID;
    uint32  Showscalp;
};

struct CharHairTexturesEntry
{
    int32   ID;
    int32   Field_0_5_3_3368_001_race;
    int32   Field_0_5_3_3368_002_gender;
    int32   Field_0_5_3_3368_003;
    int32   Field_0_5_3_3368_004_mayberacemask;
    int32   Field_0_5_3_3368_005_the_x_in_hair_xy_blp;
    int32   Field_0_5_3_3368_006;
    int32   Field_0_5_3_3368_007;
};

struct CharSectionsEntry
{
    uint32  ID;
    uint32  RaceID;
    uint32  SexID;
    uint32  BaseSection;
    uint32  VariationIndex;
    uint32  ColorIndex;
    char*   TextureName[3];
    uint32  Flags;
};

struct CharVariationsEntry
{
    uint32  RaceID;
    uint32  SexID;
    uint32  TextureHoldLayer[4];
};

struct CharacterFacialHairStylesEntry
{
    uint32  ID;
    uint32  RaceID;
    uint32  SexID;
    uint32  VariationID;
    uint32  Geoset1;
    uint32  Geoset2;
    uint32  BeardGeoset;
    uint32  MoustacheGeoset;
    uint32  SideburnGeoset;
};

struct ChatProfanityEntry
{
    int32   ID;
    char*   Text;
};

struct CinematicCameraEntry
{
    uint32  ID;
    char*   Model;
    uint32  SoundID;
    float   OriginX;
    float   OriginY;
    float   OriginZ;
    float   OriginFacing;
};

struct CreatureModelDataEntry
{
    uint32  ID;
    int32   Flags;
    char*   ModelName;
    int32   SizeClass;
    float   ModelScale;
    int32   BloodID;
    int32   FootprintTextureID;
    float   FootprintTextureLength;
    float   FootprintTextureWidth;
    float   FootprintParticleScale;
    int32   FoleyMaterialID;
    int32   FootstepShakeSize;
    int32   DeathThudShakeSize;
    int32   SoundID;
    float   CollisionWidth;
    float   CollisionHeight;
};

struct CreatureSoundDataEntry
{
    uint32  ID;
    uint32  SoundExertionID;
    uint32  SoundExertionCriticalID;
    uint32  SoundInjuryID;
    uint32  SoundInjuryCriticalID;
    uint32  SoundInjuryCrushingBlowID;
    uint32  SoundDeathID;
    uint32  SoundStunID;
    uint32  SoundStandID;
    uint32  SoundFootstepID;
    uint32  SoundAggroID;
    uint32  SoundWingFlapID;
    uint32  SoundWingGlideID;
    uint32  SoundAlertID;
    uint32  SoundFidget[4];
    uint32  CustomAttack[4];
    uint32  NPCSoundID;
    uint32  LoopSoundID;
    uint32  CreatureImpactType;
    uint32  SoundJumpStartID;
    uint32  SoundJumpEndID;
    uint32  SoundPetAttackID;
    uint32  SoundPetOrderID;
    uint32  SoundPetDismissID;
};

struct DeathThudLookupsEntry
{
    uint32  ID;
    uint32  SizeClass;
    uint32  TerrainTypeSoundID;
    uint32  SoundEntryID;
    uint32  SoundEntryIDWater;
};

struct EmotesTextDataEntry
{
    uint32  ID;
    char*   Text_lang[8];

};

struct EmotesTextSoundEntry
{
    uint32  ID;
    uint32  EmotesTextID;
    uint32  RaceID;
    uint32  SexID;
    uint32  SoundID;
};

struct EnvironmentalDamageEntry
{
    uint32  ID;
    uint32  EnumID;
    int32   VisualkitID;
};

struct ExhaustionEntry
{
    uint32  ID;
    int32   Xp;
    float   Factor;
    float   OutdoorHours;
    float   InnHours;
    char*   Name_lang[8];

    float   Threshold;
};

struct FactionGroupEntry
{
    uint32  ID;
    uint32  MaskID;
    char*   InternalName;
    char*   Name_lang[8];

};

struct FootprintTexturesEntry
{
    uint32  ID;
    char*   FootstepFilename;
};

struct FootstepTerrainLookupEntry
{
    uint32  ID;
    uint32  CreatureFootstepID;
    uint32  TerrainSoundID;
    uint32  SoundID;
    uint32  SoundIDSplash;
};

struct GMSurveyCurrentSurveyEntry
{
    uint32  LangID;
    uint32  GMSurveyID;
};

struct GMSurveyQuestionsEntry
{
    uint32  ID;
    char*   Question_lang[8];

};

struct GMSurveySurveysEntry
{
    int32   ID;
    int32   Q[10];
};

struct GMTicketCategoryEntry
{
    uint32  ID;
    char*   Category_lang[8];

};

struct GameObjectArtKitEntry
{
    int32   ID;
    char*   TextureVariation[3];
    char*   AttachModel[4];
};

struct GameTipsEntry
{
    uint32  ID;
    char*   Text_lang[8];

};

struct GroundEffectDoodadEntry
{
    uint32  ID;
    uint32  DoodadIdTag;
    char*   Doodadpath;
};

struct GroundEffectTextureEntry
{
    uint32  ID;
    int32   DoodadID[4];
    int32   Density;
    int32   Sound;
};

struct HelmetGeosetVisDataEntry
{
    uint32  ID;
    uint32  HideGeoset[5];
};

struct ItemDisplayInfoEntry
{
    uint32  ID;
    char*   ModelName[2];
    char*   ModelTexture[2];
    char*   InventoryIcon;
    int32   GeosetGroup[3];
    uint32  Flags;
    int32   SpellVisualID;
    int32   GroupSoundIndex;
    int32   HelmetGeosetVisID[2];
    char*   Texture[8];
    int32   ItemVisual;
};

struct ItemGroupSoundsEntry
{
    uint32  ID;
    uint32  Sound_1;
    uint32  Sound_2;
    uint32  Sound_3;
    uint32  Sound_4;
};

struct ItemPetFoodEntry
{
    uint32  ID;
    char*   Name_lang[8];

};

struct ItemSubClassEntry
{
    int32   ClassID;
    uint32  SubClassID;
    int32   PrerequisiteProficiency;
    int32   PostrequisiteProficiency;
    uint32  Flags;
    uint32  DisplayFlags;
    int32   WeaponParrySeq;
    int32   WeaponReadySeq;
    int32   WeaponAttackSeq;
    int32   WeaponSwingSize;
    char*   DisplayName_lang[8];

    char*   VerboseName_lang[8];

};

struct ItemSubClassMaskEntry
{
    uint32  ClassID;
    uint32  Mask;
    char*   Name_lang[8];

};

struct ItemVisualEffectsEntry
{
    uint32  ID;
    char*   Model;
};

struct ItemVisualsEntry
{
    uint32  ID;
    uint32  Effect1;
    uint32  Effect2;
    uint32  Effect3;
    uint32  Effect4;
    uint32  Effect5;
};

struct LanguageWordsEntry
{
    uint32  ID;
    uint32  LanguageID;
    char*   Word;
};

struct LanguagesEntry
{
    uint32  ID;
    char*   Name_lang[8];

};

struct LfgDungeonsEntry
{
    uint32  ID;
    char*   Name_lang[8];

    int32   MinLevel;
    int32   MaxLevel;
    int32   TypeID;
    int32   Faction;
};

struct LightEntry
{
    uint32  ID;
    uint32  ContinentID;
    float   GameCoords[3];
    float   GameFalloffStart;
    float   GameFalloffEnd;
    int32   LightParamsID[5];
};

struct LightFloatBandEntry
{
    uint32  ID;
    int32   Num;
    int32   Time[16];
    float   Data[16];
};

struct LightIntBandEntry
{
    uint32  ID;
    int32   Num;
    int32   Time[16];
    uint32  Data[16];
};

struct LightParamsEntry
{
    uint32  ID;
    int32   HighlightSky;
    uint32  LightSkyboxID;
    float   Glow;
    float   WaterShallowAlpha;
    float   WaterDeepAlpha;
    float   OceanShallowAlpha;
    float   OceanDeepAlpha;
    float   Flags;
};

struct LightSkyboxEntry
{
    uint32  ID;
    char*   Name;
};

struct LoadingScreenTaxiSplinesEntry
{
    uint32  ID;
    uint32  PathID;
    float   Locx[8];
    float   Locy[8];
    int32   LegIndex;
};

struct LoadingScreensEntry
{
    uint32  ID;
    char*   Name;
    char*   FileName;
};

struct LockTypeEntry
{
    uint32  ID;
    char*   Name_lang[8];

    char*   ResourceName_lang[8];

    char*   Verb_lang[8];

    char*   CursorName;
};

struct MaterialEntry
{
    uint32  ID;
    uint32  Flags;
    uint32  FoleySoundID;
};

struct NPCSoundsEntry
{
    uint32  ID;
    uint32  SoundID_1;
    uint32  SoundID_2;
    uint32  SoundID_3;
    uint32  SoundID_4;
};

struct NameGenEntry
{
    uint32  ID;
    char*   Name;
    uint32  RaceID;
    uint32  Sex;
};

struct NamesProfanityEntry
{
    uint32  ID;
    char*   Name;
};

struct NamesReservedEntry
{
    uint32  ID;
    char*   Name;
};

struct PackageEntry
{
    uint32  ID;
    char*   Icon;
    int32   Cost;
    char*   Name_lang_loc1;
    char*   Name_lang_loc2;
    char*   Name_lang_loc3;
    char*   Name_lang_loc4;
    char*   Name_lang_loc5;
    char*   Name_lang_loc6;
    char*   Name_lang_loc7;
    char*   Name_lang_loc8;
    uint32  Name_lang_flags;
};

struct PageTextMaterialEntry
{
    uint32  ID;
    char*   Name;
};

struct PaperDollItemFrameEntry
{
    char*   ItemButtonName;
    char*   SlotIcon;
    int32   SlotNumber;
};

struct PetLoyaltyEntry
{
    uint32  ID;
    char*   Name_lang[8];

};

struct PetPersonalityEntry
{
    uint32  ID;
    char*   Name_lang[8];

    int32   HappinessThreshold[3];
    float   HappinessDamage[3];
    float   DamageModifier[3];
};

struct QuestInfoEntry
{
    uint32  ID;
    char*   InfoName_lang[8];

};

struct ResistancesEntry
{
    uint32  ID;
    uint32  Flags;
    uint32  FizzleSoundID;
    char*   Name_lang[8];

};

struct ServerMessagesEntry
{
    uint32  ID;
    char*   Text_lang[8];

};

struct SheatheSoundLookupsEntry
{
    uint32  ID;
    uint32  ClassID;
    uint32  SubclassID;
    uint32  Material;
    uint32  CheckMaterial;
    uint32  SheatheSound;
    uint32  UnsheatheSound;
};

struct SkillCostsDataEntry
{
    uint32  ID;
    uint32  SkillCostsID;
    int32   Cost1;
    int32   Cost2;
    int32   Cost3;
};

struct SkillLineCategoryEntry
{
    uint32  ID;
    char*   Name_lang[8];

    int32   SortIndex;
};

struct SkillTiersEntry
{
    uint32  ID;
    int32   Cost[16];
    int32   Value[16];
};

struct SoundAmbienceEntry
{
    uint32  ID;
    uint32  AmbienceID[2];
};

struct SoundProviderPreferencesEntry
{
    uint32  ID;
    char*   Description;
    uint32  Flags;
    uint32  EAXEnvironmentSelection;
    float   EAXDecayTime;
    float   EAX2EnvironmentSize;
    float   EAX2EnvironmentDiffusion;
    int32   EAX2Room;
    int32   EAX2RoomHF;
    float   EAX2DecayHFRatio;
    int32   EAX2Reflections;
    float   EAX2ReflectionsDelay;
    int32   EAX2Reverb;
    float   EAX2ReverbDelay;
    float   EAX2RoomRolloff;
    float   EAX2AirAbsorption;
    int32   EAX3RoomLF;
    float   EAX3DecayLFRatio;
    float   EAX3EchoTime;
    float   EAX3EchoDepth;
    float   EAX3ModulationTime;
    float   EAX3ModulationDepth;
    float   EAX3HFReference;
    float   EAX3LFReference;
};

struct SoundSamplePreferencesEntry
{
    uint32  ID;
    int32   EAX2SampleDirect;
    int32   EAX2SampleDirectHF;
    int32   EAX2SampleRoom;
    int32   EAX2SampleRoomHF;
    float   EAX2SampleObstruction;
    float   EAX2SampleObstructionLFRatio;
    float   EAX2SampleOcclusion;
    float   EAX2SampleOcclusionLFRatio;
    float   EAX2SampleOcclusionRoomRatio;
    float   EAX2SampleRoomRolloffFactor;
    float   EAX2SampleAirAbsorptionFactor;
    int32   EAX2SampleOutsideVolumeHF;
    float   EAX3SampleOcclusionDirectRatio;
    float   EAX3SampleExclusion;
    float   EAX3SampleExclusionLFRatio;
    float   EAX3SampleDopplerFactor;
};

struct SoundWaterTypeEntry
{
    uint32  ID;
    uint32  SoundType;
    uint32  SoundSubtype;
    uint32  SoundID;
};

struct SpamMessagesEntry
{
    uint32  ID;
    char*   Text;
};

struct SpellCategoryEntry
{
    uint32  ID;
    uint32  Flags;
};

struct SpellChainEffectsEntry
{
    uint32  ID;
    float   AvgSegLen;
    float   Width;
    float   NoiseScale;
    float   TexCoordScale;
    int32   SegDuration;
    int32   SegDelay;
    char*   Texture;
};

struct SpellDispelTypeEntry
{
    uint32  ID;
    char*   Name_lang[8];

    uint32  Mask;
    char*   InternalName;
};

struct SpellEffectCameraShakesEntry
{
    uint32  ID;
    uint32  CameraShake_1;
    uint32  CameraShake_2;
    uint32  CameraShake_3;
};

struct SpellIconEntry
{
    uint32  ID;
    char*   TextureFilename;
};

struct SpellMechanicEntry
{
    uint32  ID;
    char*   StateName_lang[8];

};

struct SpellVisualEntry
{
    uint32  ID;
    uint32  PrecastKit;
    uint32  CastKit;
    uint32  ImpactKit;
    uint32  StateKit;
    uint32  ChannelKit;
    uint32  HasMissile;
    uint32  MissileModel;
    uint32  MissilePathType;
    uint32  MissileDestinationAttachment;
    uint32  MissileSound;
    uint32  HasAreaEffect;
    uint32  AreaModel;
    uint32  AreaKit;
    uint32  AnimEventSoundID;
    uint32  Flags;
};

struct SpellVisualEffectNameEntry
{
    int32   ID;
    char*   Name;
    char*   FileName;
    int32   SpecialAttachPoint;
    float   AreaEffectSize;
};

struct SpellVisualKitEntry
{
    uint32  ID;
    int32   KitType;
    int32   AnimID;
    int32   HeadEffect;
    int32   ChestEffect;
    int32   BaseEffect;
    int32   LeftHandEffect;
    int32   RightHandEffect;
    int32   BreathEffect;
    int32   SpecialEffect[3];
    int32   WorldEffect;
    int32   SoundID;
    int32   ShakeID;
    int32   CharProc[4];
    float   CharParamZero[4];
    float   CharParamOne[4];
    float   CharParamTwo[4];
    float   CharParamThree[4];
};

struct SpellVisualPrecastTransitionsEntry
{
    int32   ID;
    char*   PrecastLoadAnimName;
    char*   PrecastHoldAnimName;
};

struct Startup_StringsEntry
{
    uint32  ID;
    char*   Name;
    char*   Message_lang[8];

};

struct StationeryEntry
{
    uint32  ID;
    uint32  ItemID;
    char*   Texture;
    uint32  Flags;
};

struct StringLookupsEntry
{
    uint32  ID;
    char*   String;
};

struct TerrainTypeEntry
{
    uint32  TerrainID;
    char*   TerrainDesc;
    int32   FootstepSprayRun;
    int32   FootstepSprayWalk;
    uint32  SoundID;
    uint32  Flags;
};

struct TerrainTypeSoundsEntry
{
    uint32  ID;
};

struct TransportAnimationEntry
{
    uint32  ID;
    uint32  TransportID;
    uint32  TimeIndex;
    float   PosX;
    float   PosY;
    float   PosZ;
    uint32  SequenceID;
};

struct UISoundLookupsEntry
{
    uint32  ID;
    uint32  SoundID;
    char*   SoundName;
};

struct UnitBloodEntry
{
    uint32  ID;
    uint32  CombatBloodSpurtFront[2];
    uint32  CombatBloodSpurtBack[2];
    char*   GroundBlood[5];
};

struct UnitBloodLevelsEntry
{
    uint32  ID;
    uint32  Violencelevel_1;
    uint32  Violencelevel_2;
    uint32  Violencelevel_3;
};

struct VideoHardwareEntry
{
    uint32  ID;
    uint32  VendorID;
    uint32  DeviceID;
    uint32  FarclipIdx;
    uint32  TerrainLODDistIdx;
    int32   TerrainShadowLOD;
    uint32  DetailDoodadDensityIdx;
    int32   DetailDoodadAlpha;
    uint32  AnimatingDoodadIdx;
    int32   Trilinear;
    int32   NumLights;
    int32   Specularity;
    uint32  WaterLODIdx;
    uint32  ParticleDensityIdx;
    uint32  UnitDrawDistIdx;
    uint32  SmallCullDistIdx;
    uint32  ResolutionIdx;
    int32   BaseMipLevel;
    char*   OglOverrides;
    char*   D3dOverrides;
    int32   FixLag;
    int32   Multisample;
};

struct VocalUISoundsEntry
{
    uint32  ID;
    uint32  VocalUIEnum;
    uint32  RaceID;
    uint32  NormalSoundID[2];
    uint32  PissedSoundID[2];
};

struct WeaponImpactSoundsEntry
{
    uint32  ID;
    uint32  WeaponSubClassID;
    uint32  ParrySoundType;
    uint32  ImpactSoundID[10];
    uint32  CritImpactSoundID[10];
};

struct WeaponSwingSounds2Entry
{
    uint32  ID;
    int32   SwingType;
    int32   Crit;
    uint32  SoundID;
};

struct WorldMapContinentEntry
{
    uint32  ID;
    uint32  MapID;
    int32   LeftBoundary;
    int32   RightBoundary;
    int32   TopBoundary;
    int32   BottomBoundary;
    float   ContinentOffset[2];
    float   Scale;
    float   TaxiMin[2];
    float   TaxiMax[2];
};

struct WorldMapOverlayEntry
{
    uint32  ID;
    int32   MapAreaID;
    int32   AreaID[4];
    int32   MapPointX;
    int32   MapPointY;
    char*   TextureName;
    int32   TextureWidth;
    int32   TextureHeight;
    int32   OffsetX;
    int32   OffsetY;
    int32   HitRectTop;
    int32   HitRectLeft;
    int32   HitRectBottom;
    int32   HitRectRight;
};

struct WorldStateUIEntry
{
    uint32  ID;
    int32   MapID;
    int32   AreaID;
    char*   Icon;
    char*   String_lang[8];

    char*   Tooltip_lang[8];

    int32   FactionID;
    int32   StateVariable;
    int32   Type;
    char*   DynamicIcon;
    char*   DynamicTooltip_lang[8];

    char*   ExtendedUI;
    int32   ExtendedUIStateVariable[3];
};

struct WowError_StringsEntry
{
    int32   ID;
    int32   Name;
    char*   Description_lang[8];

};

struct ZoneIntroMusicTableEntry
{
    uint32  ID;
    char*   Name;
    uint32  SoundID;
    int32   Priority;
    int32   MinDelayMinutes;
};

struct ZoneMusicEntry
{
    uint32  ID;
    char*   SetName;
    int32   SilenceIntervalMin[2];
    int32   SilenceIntervalMax[2];
    uint32  Sounds[2];
};
