#include "stdafx.h"
#include "Beatmap.hpp"
#include "json.hpp"
#include "Shared/Profiling.hpp"
#include "Shared/StringEncodingDetector.hpp"
#include "Shared/StringEncodingConverter.hpp"
#include <limits>

bool checkedGet(const nlohmann::json obj, char* field, String& target)
{
	if (obj.contains(field) && obj.at(field).is_string())
	{
		obj.at(field).get_to(target);
		return true;
	}
	return false;
}

bool checkedGet(const nlohmann::json obj, char* field, int& target)
{
	if (obj.contains(field) && obj.at(field).is_number_integer())
	{
		obj.at(field).get_to(target);
		return true;
	}
	return false;
}

bool checkedGet(const nlohmann::json obj, char* field, uint8& target)
{
	if (obj.contains(field) && obj.at(field).is_number_unsigned())
	{
		obj.at(field).get_to(target);
		return true;
	}
	return false;
}

bool checkedGet(const nlohmann::json obj, char* field, uint32& target)
{
	if (obj.contains(field) && obj.at(field).is_number_unsigned())
	{
		obj.at(field).get_to(target);
		return true;
	}
	return false;
}
bool checkedGet(const nlohmann::json obj, char* field, float& target)
{
	if (obj.contains(field) && obj.at(field).is_number_float())
	{
		obj.at(field).get_to(target);
		return true;
	}
	return false;
}
bool checkedGet(const nlohmann::json obj, char* field, double& target)
{
	if (obj.contains(field) && obj.at(field).is_number_float())
	{
		obj.at(field).get_to(target);
		return true;
	}
	return false;
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

	auto kson = nlohmann::json::parse(fileString);

	auto meta = kson["meta"];
	auto difficulty = meta["difficulty"];
	auto beat = kson["beat"];
	auto audio = kson["audio"];
	auto bgm = audio["bgm"];
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
	return false;
}