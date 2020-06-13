#include "stdafx.h"
#include "Beatmap.hpp"
#include "json.hpp"
#include "kson.hpp"
#include "Shared/Profiling.hpp"
#include "Shared/StringEncodingDetector.hpp"
#include "Shared/StringEncodingConverter.hpp"
#include <limits>
#include <climits>

bool checkedGet(const nlohmann::json& obj, const char* field, String& target)
{
	if (obj.contains(field) && obj.at(field).is_string())
	{
		obj.at(field).get_to(target);
		return true;
	}
	return false;
}

bool checkedGet(const nlohmann::json& obj, const char* field, int& target)
{
	if (obj.contains(field) && obj.at(field).is_number_integer())
	{
		obj.at(field).get_to(target);
		return true;
	}
	return false;
}

bool checkedGet(const nlohmann::json& obj, const char* field, uint8& target)
{
	if (obj.contains(field) && obj.at(field).is_number_unsigned())
	{
		obj.at(field).get_to(target);
		return true;
	}
	return false;
}

bool checkedGet(const nlohmann::json& obj, const char* field, uint32& target)
{
	if (obj.contains(field) && obj.at(field).is_number_unsigned())
	{
		obj.at(field).get_to(target);
		return true;
	}
	return false;
}
bool checkedGet(const nlohmann::json& obj, const char* field, float& target)
{
	if (obj.contains(field) && obj.at(field).is_number_float())
	{
		obj.at(field).get_to(target);
		return true;
	}
	return false;
}
bool checkedGet(const nlohmann::json& obj, const char* field, double& target)
{
	if (obj.contains(field) && obj.at(field).is_number_float())
	{
		obj.at(field).get_to(target);
		return true;
	}
	return false;
}

inline double beatInMs(double bpm) {
	return 60000.0 / bpm;
}

inline double tickInMs(double bpm, uint32 resolution)
{
	return beatInMs(bpm) / (double)resolution;
}

inline double msFromTicks(int64 ticks, double bpm, uint32 resolution)
{
	return tickInMs(bpm, resolution) * (double)ticks;
}

MapTime MapTimeFromTicks(const Vector<ByPulse<double>>& bpms, uint32 resolution, int64 tick)
{
	if (bpms.empty())
		return 0;

	auto prev = bpms.front();
	double ret = 0.0;

	for (auto& bpm : bpms)
	{
		if (bpm.tick > tick)
			break;
		ret += msFromTicks(bpm.tick - prev.tick, prev.value, resolution);
	}
	ret += msFromTicks(tick - prev.tick, prev.value, resolution);
	return static_cast<MapTime>(Math::Round(ret));
}

ObjectState* StateFromButtonInterval(const nlohmann::json::value_type& button, int index, const Vector<ByPulse<double>>& bpms, uint32 resolution)
{
	Interval interval;
	interval.length = 0;
	checkedGet(button, "y", interval.tick);
	checkedGet(button, "l", interval.length);
	if (interval.length == 0)
	{
		ButtonObjectState* obj = new ButtonObjectState();
		obj->time = MapTimeFromTicks(bpms, resolution, interval.tick);
		obj->index = index;
		obj->hasSample = false;
		obj->sampleIndex = 0;
		obj->sampleVolume = 0;
		return (ObjectState*)obj;
	}
	else {
		HoldObjectState* obj = new HoldObjectState();
		obj->time = MapTimeFromTicks(bpms, resolution, interval.tick);
		obj->index = index;
		obj->duration = MapTimeFromTicks(bpms, resolution, interval.tick + interval.length) - obj->time;
		obj->effectType = EffectType::None;
		obj->next = nullptr;
		obj->prev = nullptr;
		return (ObjectState*)obj;
	}

	//TODO
	return nullptr;
}


bool Beatmap::m_ProcessKSON(BinaryStream& input, bool metadataOnly)
{
	ProfilerScope $("Read kson");
	//process meta fields
	StringEncoding chartEncoding = StringEncoding::UTF8;
	//TODO: Encoding detection? (spec says always utf8 /wo BOM)
	//TODO: Add log entries for false returns

	String fileString;
	TextStream::ReadAll(input, fileString);
	Map<int, ObjectState*> btnMap[6];


	const auto& kson = nlohmann::json::parse(fileString);

	auto& meta = kson["meta"];
	auto& difficulty = meta["difficulty"];
	auto& beat = kson["beat"];
	auto& audio = kson["audio"];
	auto& bgm = audio["bgm"];
	if (!checkedGet(meta, "title", m_settings.title))
		return false;
	if (!checkedGet(meta, "artist", m_settings.artist))
		return false;

	checkedGet(meta, "chart_author", m_settings.effector);
	if (!checkedGet(difficulty, "idx", m_settings.difficulty))
		return false;

	checkedGet(meta, "jacket_filename", m_settings.jacketPath);
	checkedGet(meta, "jacket_author", m_settings.illustrator);
	checkedGet(meta, "level", m_settings.level);
	if (!checkedGet(meta, "disp_bpm", m_settings.bpm))
	{
		//get from beat data
		if (!beat["bpm"].is_array())
			return false;

		float minBpm, maxBpm;
		minBpm = std::numeric_limits<float>::max();
		maxBpm = std::numeric_limits<float>::min();

		for (auto& bpm : beat["bpm"])
		{
			float currentBpm;
			bpm["v"].get_to(currentBpm);
			minBpm = Math::Min(currentBpm, minBpm);
			maxBpm = Math::Max(currentBpm, maxBpm);
		}
		if (maxBpm == std::numeric_limits<float>::max())
			return false;

		if (minBpm == maxBpm)
			m_settings.bpm = Utility::Sprintf("%.1f", minBpm);
		else 
			m_settings.bpm = Utility::Sprintf("%.1f-%.1f", minBpm, maxBpm);
	}

	checkedGet(bgm, "filename", m_settings.audioNoFX);
	checkedGet(bgm, "offset", m_settings.offset);
	checkedGet(bgm, "preview_duration", m_settings.previewDuration);
	checkedGet(bgm, "preview_offset", m_settings.previewOffset);
	checkedGet(bgm, "vol", m_settings.musicVolume);
	

	if (metadataOnly)
		return true;

	//process chart data
	Vector<ByPulse<double>> bpm_entries;
	uint32 resolution;

	if (!checkedGet(beat, "resolution", resolution))
		return false;
	if (!beat["bpm"].is_array())
		return false;
	for (auto& bpm : beat["bpm"])
	{
		ByPulse<double> currentBpm;
		bpm["v"].get_to(currentBpm.value);
		bpm["y"].get_to(currentBpm.tick);
		bpm_entries.push_back(currentBpm);
	}
	//ensure sorted
	bpm_entries.Sort([](const ByPulse<double>& a, const ByPulse<double>& b) { return a.tick > b.tick; });

	if (!kson.contains("note") || kson["note"].is_null())
		return false;
	
	auto& note = kson["note"];
	auto& bt = note["bt"];
	auto& fx = note["fx"];
	auto& laser = note["laser"];

	//BTs
	if (bt.is_array())
	{
		for (size_t i = 0; i < 4; i++)
		{
			auto lane = bt[i];
			if (!lane.is_array())
				continue;

			int laneIdx = 0;
			for (auto& button : lane)
			{
				ObjectState* newState = StateFromButtonInterval(button, i, bpm_entries, resolution);
				if (newState)
				{
					m_objectStates.Add(newState);
					btnMap[i].Add(laneIdx, newState);
				}
				laneIdx++;
			}
		}
	}

	//FXs
	if (fx.is_array())
	{
		for (size_t i = 0; i < 2; i++)
		{
			auto lane = fx[i];
			if (!lane.is_array())
				continue;

			int laneIdx = 0;
			for (auto& button : lane)
			{
				ObjectState* newState = StateFromButtonInterval(button, i + 4, bpm_entries, resolution);
				if (newState)
				{
					m_objectStates.Add(newState);
					btnMap[i + 4].Add(laneIdx, newState);
				}
				laneIdx++;
			}
		}
	}

	//Lasers
	if (laser.is_array())
	{
		for (size_t i = 0; i < 2; i++)
		{
			auto lane = laser[i];
			if (!lane.is_array())
				continue;

			for (auto& segment : lane)
			{
				if (!segment["v"].is_array())
					continue;

				int startTick;
				uint8 wide;
				checkedGet(segment, "wide", wide);
				checkedGet(segment, "y", startTick);
				LaserObjectState* prev = nullptr;
				int prevRelative = INT_MIN;
				double prevValue = 0;
				int laneIdx = 0;
				for (auto& gp : segment["v"])
				{
					double valueFinal;
					double value;
					int relativeTick;
					checkedGet(gp, "v", value);
					checkedGet(gp, "ry", relativeTick);

					if (prevRelative != INT_MIN) //add segment
					{
						LaserObjectState* obj = new LaserObjectState();
						if (wide == 2)
							obj->flags |= LaserObjectState::flag_Extended;
						obj->time = MapTimeFromTicks(bpm_entries, resolution, startTick + prevRelative);
						obj->duration = MapTimeFromTicks(bpm_entries, resolution, startTick + relativeTick) - obj->time;
						obj->tick = startTick + prevRelative;
						obj->index = i;
						obj->points[0] = prevValue;
						obj->points[1] = value;
						if(prev)
							prev->next = obj;
						obj->prev = prev;

						prev = obj;
						m_objectStates.Add((ObjectState*)obj);
					}
					prevRelative = relativeTick;
					prevValue = value;
					if (checkedGet(gp, "vf", valueFinal) && value != valueFinal) //add slam
					{
						LaserObjectState* obj = new LaserObjectState();
						obj->flags |= LaserObjectState::flag_Instant;
						if (wide == 2)
							obj->flags |= LaserObjectState::flag_Extended;
						obj->time = MapTimeFromTicks(bpm_entries, resolution, startTick + relativeTick);
						obj->tick = startTick + relativeTick;
						obj->index = i;
						obj->points[0] = value;
						obj->points[1] = valueFinal;
						if (prev) {
							prev->next = obj;
							obj->prev = prev;
						}
						prev = obj;
						m_objectStates.Add((ObjectState*)obj);
						
						prevRelative = relativeTick;
						prevValue = valueFinal;
					}
					laneIdx++;
				}
			}
		}
	}

	//Effect entries
	auto& audioEffect = audio["audio_effect"];
	auto& effectNoteEvent = audioEffect["note_event"];
	EffectTypeMap effectMap = EffectTypeMap();

	if (effectNoteEvent.is_object())
	{
		for (auto& effect : effectNoteEvent.items())
		{
			String effectTypeName = effect.key();
			auto* effectType = effectMap.FindEffectType(effectTypeName);
			if (effectType == nullptr)
			{
				//TODO: Deal with custom effects
				Logf("Unknown effect type: %s", Logger::Warning, *effectTypeName);
				continue;
			}

			auto& fx = effect.value()["fx"];
			if (fx.is_array())
			{
				for (auto& fxEntry : fx)
				{
					int laneIdx, noteIdx;
					fxEntry.at("idx").get_to(noteIdx);
					fxEntry.at("lane").get_to(laneIdx);
					laneIdx += 4;
					int params[2] = { 0, 0 };
					params[0] = effectMap.GetDefaultParam(*effectType);
					//set params
					if (fxEntry.contains("v") && fxEntry.at("v").is_object())
					{
						for (auto& param : fxEntry.at("v").items())
						{
							//TODO: Deal with multiple values in value array
							if (!param.value().is_array())
								continue;
							String value;
							param.value().at(0).get_to(value);
							
							switch (*effectType)
							{
							case EffectType::Bitcrush:
								if (param.key() == "reduction")
									params[0] = std::stoi(value);
								break;
							case EffectType::TapeStop:
								if (param.key() == "speed")
									params[0] = std::stoi(value);
								break;
							default:
								break;
							}
						}
					}
					if (laneIdx < 0 || laneIdx > 5)
					{
						Logf("Out of bounds button lane: %d", Logger::Warning, laneIdx);
						continue;
					}
					if (btnMap[laneIdx].Contains(noteIdx))
					{
						ObjectState* obj = btnMap[laneIdx][noteIdx];
						if (obj->type == ObjectType::Hold)
						{
							HoldObjectState* button = (HoldObjectState*)obj;
							button->effectType = *effectType;
							for (size_t i = 0; i < 2; i++)
							{
								button->effectParams[i] = static_cast<int16>(params[i]);
							}
						}
					}
				}
			}
		}
	}

	for (auto& bpm : bpm_entries)
	{
		TimingPoint* newTp = new TimingPoint();
		newTp->time = MapTimeFromTicks(bpm_entries, resolution, bpm.tick);
		newTp->beatDuration = beatInMs(bpm.value);
		//TODO:
		newTp->denominator = 4;
		newTp->numerator = 4;
		newTp->tickrateOffset = 0;
		m_timingPoints.push_back(newTp);
	}

	ZoomControlPoint* firstControlPoints[5] = { nullptr };
	//process zoom control points

	//fill in missing zoom control points
	for (int i = 0; i < sizeof(firstControlPoints) / sizeof(ZoomControlPoint*); i++)
	{
		ZoomControlPoint* point = firstControlPoints[i];
		if (!point)
			continue;

		ZoomControlPoint* dup = new ZoomControlPoint();
		dup->index = point->index;
		dup->zoom = point->zoom;
		dup->time = INT32_MIN;

		m_zoomControlPoints.insert(m_zoomControlPoints.begin(), dup);
	}

	// Add First Lane Toggle Point
	LaneHideTogglePoint* startLaneTogglePoint = new LaneHideTogglePoint();
	startLaneTogglePoint->time = 0;
	startLaneTogglePoint->duration = 1;
	m_laneTogglePoints.Add(startLaneTogglePoint);

	//process lane toggles


	ObjectState::SortArray(m_objectStates);
	return true;
}