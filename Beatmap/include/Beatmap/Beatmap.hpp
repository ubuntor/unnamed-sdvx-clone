#pragma once
#include "BeatmapObjects.hpp"
#include "AudioEffects.hpp"

/* Global settings stored in a beatmap */
struct BeatmapSettings
{
	static bool StaticSerialize(BinaryStream& stream, BeatmapSettings*& settings);

	// Basic song meta data
	String title;
	String artist;
	String effector;
	String illustrator;
	String tags;
	// Reported BPM range by the map
	String bpm;
	// Offset in ms for the map to start
	MapTime offset;
	// Both audio tracks specified for the map / if any is set
	String audioNoFX;
	String audioFX;
	// Path to the jacket image
	String jacketPath;
	// Path to the background and foreground shader files
	String backgroundPath;
	String foregroundPath;

	// Level, as indicated by map creator
	uint8 level;

	// Difficulty, as indicated by map creator
	uint8 difficulty;

	// Total, total gauge gained when played perfectly
	uint16 total = 0;

	// Preview offset
	MapTime previewOffset;
	// Preview duration
	MapTime previewDuration;

	// Initial audio settings
	float slamVolume = 1.0f;
	float laserEffectMix = 1.0f;
	float musicVolume = 1.0f;
	EffectType laserEffectType = EffectType::PeakingFilter;
};

class EffectTypeMap
{
	// Custom effect types
	uint16 m_customEffectTypeID = (uint16)EffectType::UserDefined0;

public:
	EffectTypeMap()
	{
		// Add common effect types
		effectTypes["None"] = EffectType::None;
		effectTypes["Retrigger"] = EffectType::Retrigger;
		effectTypes["Flanger"] = EffectType::Flanger;
		effectTypes["Phaser"] = EffectType::Phaser;
		effectTypes["Gate"] = EffectType::Gate;
		effectTypes["TapeStop"] = EffectType::TapeStop;
		effectTypes["BitCrusher"] = EffectType::Bitcrush;
		effectTypes["Wobble"] = EffectType::Wobble;
		effectTypes["SideChain"] = EffectType::SideChain;
		effectTypes["Echo"] = EffectType::Echo;
		effectTypes["Panning"] = EffectType::Panning;
		effectTypes["PitchShift"] = EffectType::PitchShift;
		effectTypes["LPF"] = EffectType::LowPassFilter;
		effectTypes["HPF"] = EffectType::HighPassFilter;
		effectTypes["PEAK"] = EffectType::PeakingFilter;
		effectTypes["SwitchAudio"] = EffectType::SwitchAudio;
	}

	static const Map<EffectType, int16> CreateDefaults() {
		Map<EffectType, int16> defaults;
		defaults[EffectType::Bitcrush] = 4;
		defaults[EffectType::Gate] = 8;
		defaults[EffectType::Retrigger] = 8;
		defaults[EffectType::Phaser] = 2000;
		defaults[EffectType::Flanger] = 2000;
		defaults[EffectType::Wobble] = 12;
		defaults[EffectType::SideChain] = 8;
		defaults[EffectType::TapeStop] = 50;
		return defaults;
	}

	// Only checks if a mapping exists and returns this, or None
	const EffectType* FindEffectType(const String& name) const
	{
		return effectTypes.Find(name);
	}

	// Adds or returns the enum value mapping to this effect
	EffectType FindOrAddEffectType(const String& name)
	{
		EffectType* id = effectTypes.Find(name);
		if (!id)
			return effectTypes.Add(name, (EffectType)m_customEffectTypeID++);
		return *id;
	};

	int16 GetDefaultParam(EffectType type) {
		if (!defaultEffectParams.Contains(type))
			return 0;
		return defaultEffectParams.at(type);
	}

	const Map<EffectType, int16> defaultEffectParams = CreateDefaults();
	Map<String, EffectType> effectTypes;
};

/*
	Generic beatmap format, Can either load it's own format or KShoot maps
*/
class Beatmap : public Unique
{
public:
	enum class Format
	{
		KSH,
		KSON
	};

	virtual ~Beatmap();
	Beatmap() = default;
	Beatmap(Beatmap&& other);
	Beatmap& operator=(Beatmap&& other);

	bool Load(BinaryStream& input, Format format, bool metadataOnly = false);
	// Saves the map as it's own format
	bool Save(BinaryStream& output) const;

	// Returns the settings of the map, contains metadata + song/image paths.
	const BeatmapSettings& GetMapSettings() const;

	// Vector of timing points in the map, sorted by when they appear in the map
	// Must keep the beatmap class instance alive for these to stay valid
	// Can contain multiple objects at the same time
	const Vector<TimingPoint*>& GetLinearTimingPoints() const;
	// Vector of chart stops in the chart, sorted by when they appear in the map
	// Must keep the beatmap class instance alive for these to stay valid
	// Can contain multiple objects at the same time
	const Vector<ChartStop*>& GetLinearChartStops() const;
	// Vector of objects in the map, sorted by when they appear in the map
	// Must keep the beatmap class instance alive for these to stay valid
	// Can contain multiple objects at the same time
	const Vector<ObjectState*>& GetLinearObjects() const;
	// Vector of zoom control points in the map, sorted by when they appear in the map
	// Must keep the beatmap class instance alive for these to stay valid
	// Can contain multiple objects at the same time
	const Vector<ZoomControlPoint*>& GetZoomControlPoints() const;

	const Vector<LaneHideTogglePoint*>& GetLaneTogglePoints() const;

	const Vector<String>& GetSamplePaths() const;

	const Vector<String>& GetSwitchablePaths() const;

	// Retrieves audio effect settings for a given button id
	AudioEffect GetEffect(EffectType type) const;
	// Retrieves audio effect settings for a given filter effect id
	AudioEffect GetFilter(EffectType type) const;

	// Get the timing of the last (non-event) object
	MapTime GetLastObjectTime() const;

	// Measure -> Time
	MapTime GetMapTimeFromMeasureInd(int measure) const;
	// Time -> Measure
	int GetMeasureIndFromMapTime(MapTime time) const;

private:
	bool m_ProcessKShootMap(BinaryStream& input, bool metadataOnly);
	bool m_ProcessKSON(BinaryStream& input, bool metadataOnly);
	bool m_Serialize(BinaryStream& stream, bool metadataOnly);

	Map<EffectType, AudioEffect> m_customEffects;
	Map<EffectType, AudioEffect> m_customFilters;

	Vector<TimingPoint*> m_timingPoints;
	Vector<ChartStop*> m_chartStops;
	Vector<LaneHideTogglePoint*> m_laneTogglePoints;
	Vector<ObjectState*> m_objectStates;
	Vector<ZoomControlPoint*> m_zoomControlPoints;
	Vector<String> m_samplePaths;
	Vector<String> m_switchablePaths;
	BeatmapSettings m_settings;
};