#include "stdafx.h"
#include "Application.hpp"
#include "SkinConfig.hpp"
#include "json.hpp"
#include "fifo_map.hpp"

using namespace nlohmann;


// https://github.com/nlohmann/json/issues/485#issuecomment-333652309
// A workaround to give to use fifo_map as map, we are just ignoring the 'less' compare
template<class K, class V, class dummy_compare, class A>
using workaround_fifo_map = fifo_map<K, V, fifo_map_compare<K>, A>;

SkinConfig::SkinConfig(String skin)
{
	m_skin = skin;


	using ordered_json = basic_json<workaround_fifo_map>;

	Map<String, SkinSetting::Type> inputModeMap = {
		{ "selection", SkinSetting::Type::Selection },
		{ "bool", SkinSetting::Type::Boolean },
		{ "int", SkinSetting::Type::Integer},
		{ "float", SkinSetting::Type::Float},
		{ "label", SkinSetting::Type::Label},
		{ "text", SkinSetting::Type::Text},
		{ "color", SkinSetting::Type::Color},
	};

	File defFile;
	String configPath = Path::Normalize(Path::Absolute("skins/" + skin + "/config-definitions.json"));
	//Log(configPath);
	if(defFile.OpenRead(configPath))
	{
		auto showError = [](String message)
		{
			g_gameWindow->ShowMessageBox("Skin config parser error.", message, 0);
		};
		

		Buffer buf(defFile.GetSize());
		defFile.Read(buf.data(), buf.size());
		String jsonData((char*)buf.data(), buf.size());
		ordered_json definitions;
		///TODO: Don't use exceptions
		try
		{
			definitions = ordered_json::parse(*jsonData);
		}
		catch (const std::exception& e)
		{
			showError(e.what());
			return;
		}

		for (auto entry : definitions.items())
		{
			SkinSetting newsetting;
			String key = entry.key();
			if (key.compare(0, 9, "separator") == 0)
			{
				newsetting.type = SkinSetting::Type::Separator;
				m_settings.Add(newsetting);
				continue;
			}
			auto values = entry.value();
			newsetting.key = entry.key();
			String def;
			String type;
			if (!values.contains("type"))
			{
				showError(Utility::Sprintf("No type specified for: \"%s\"", key));
				continue;
			}
			
			values.at("type").get_to(type);

			if (!inputModeMap.Contains(type))
			{
				showError(Utility::Sprintf("Unknown type \"%s\" used for \"%s\"", type, key));
				continue;
			}

			if (values.contains("label"))
			{
				values.at("label").get_to(newsetting.label);
			}
			else
			{
				newsetting.label = entry.key();
			}

			newsetting.type = inputModeMap.at(type);

			if (!values.contains("default") && newsetting.type != SkinSetting::Type::Label)
			{
				showError(Utility::Sprintf("No default value specified for: \"%s\"", key));
				continue;
			}

			switch (newsetting.type)
			{
			case SkinSetting::Type::Selection:
				values.at("default").get_to(def);
				newsetting.selectionSetting.def = strdup(*def);
				newsetting.selectionSetting.numOptions = values.at("values").size();
				newsetting.selectionSetting.options = new String[newsetting.selectionSetting.numOptions];
				for (size_t i = 0; i < newsetting.selectionSetting.numOptions; i++)
				{
					values.at("values").at(i).get_to(newsetting.selectionSetting.options[i]);
				}
				break;

			case SkinSetting::Type::Boolean:
				newsetting.boolSetting.def = values.at("default");
				break;

			case SkinSetting::Type::Text:
				newsetting.textSetting.secret = false;
				values.at("default").get_to(def);
				newsetting.textSetting.def = strdup(*def);
				if (values.contains("secret"))
				{
					values.at("secret").get_to(newsetting.textSetting.secret);
				}
				break;

			case SkinSetting::Type::Integer:
				newsetting.intSetting.def = values.at("default");
				newsetting.intSetting.max = values.at("max");
				newsetting.intSetting.min = values.at("min");
				break;

			case SkinSetting::Type::Float:
				newsetting.floatSetting.def = values.at("default");
				newsetting.floatSetting.max = values.at("max");
				newsetting.floatSetting.min = values.at("min");
				break;

			case SkinSetting::Type::Color:
				values.at("default").get_to(def);
				ColorConfigEntry ce;
				ce.FromString(def);
				newsetting.colorSetting.def = new Color(ce.data);
				if (values.contains("hsv"))
				{
					newsetting.colorSetting.hsv = values.at("hsv");
				}
				else
				{
					newsetting.colorSetting.hsv = false;
				}
				break;
			}
			m_settings.Add(newsetting);
		}
	}

    InitDefaults();
	Load(Path::Normalize(Path::Absolute("skins/" + skin + "/skin.cfg")));
}

SkinConfig::~SkinConfig()
{
	for (auto& it : m_keys)
	{
		m_reverseKeys.Add(it.second, it.first);
	}
	Save(Path::Normalize(Path::Absolute("skins/" + m_skin + "/skin.cfg")));
	for (auto s : m_settings)
	{
		if (s.type == SkinSetting::Type::Color)
		{
			if (s.colorSetting.def)
			{
				delete s.colorSetting.def;
				s.textSetting.def = nullptr;
			}
		}
		else if (s.type == SkinSetting::Type::Selection)
		{
			if (s.selectionSetting.def)
			{
				free(s.selectionSetting.def);
				s.selectionSetting.def = nullptr;
			}
			if (s.selectionSetting.options)
			{
				delete[] s.selectionSetting.options;
				s.selectionSetting.options = nullptr;
			}

		}
		else if (s.type == SkinSetting::Type::Text)
		{
			if (s.textSetting.def)
			{
				free(s.textSetting.def);
				s.textSetting.def = nullptr;
			}
		}
	}
}

bool SkinConfig::IsSet(String key) const
{
	return (m_keys.Contains(key) && m_entries.Contains(m_keys.at(key)));
}

const Vector<SkinSetting>& SkinConfig::GetSettings() const
{
	return m_settings;
}

void SkinConfig::InitDefaults()
{
	for (auto& setting : m_settings)
	{
		String def;
		switch (setting.type)
		{
		case SkinSetting::Type::Selection:
			def = setting.selectionSetting.def;
			Set(setting.key, def);
			break;
		case SkinSetting::Type::Boolean:
			Set(setting.key, setting.boolSetting.def);
			break;
		case SkinSetting::Type::Integer:
			Set(setting.key, setting.intSetting.def);
			break;
		case SkinSetting::Type::Float:
			Set(setting.key, setting.floatSetting.def);
			break;
		case SkinSetting::Type::Text:
			def = setting.textSetting.def;
			Set(setting.key, def);
			break;
		case SkinSetting::Type::Color:
			Set(setting.key, *setting.colorSetting.def);
			break;
		}

	}
}

int32 SkinConfig::GetInt(String key) const
{
	return GetEnsure<IntConfigEntry>(key)->data;
}
float SkinConfig::GetFloat(String key) const
{
	return GetEnsure<FloatConfigEntry>(key)->data;
}
String SkinConfig::GetString(String key) const
{
	return GetEnsure<StringConfigEntry>(key)->data;
}
bool SkinConfig::GetBool(String key) const
{
	return GetEnsure<BoolConfigEntry>(key)->data;
}

Color SkinConfig::GetColor(String key) const
{
	return GetEnsure<ColorConfigEntry>(key)->data;
}

IConfigEntry* SkinConfig::GetEntry(String key) const
{
	if(!m_keys.Contains(key))
		return nullptr;

	return m_entries.at(m_keys.at(key));
}


void SkinConfig::Set(String key, const int32& value)
{
	int32& dst = SetEnsure<IntConfigEntry>(key)->data;
	if (dst != value)
	{
		dst = value;
		m_dirty = true;
	}
}
void SkinConfig::Set(String key, const float& value)
{
	float& dst = SetEnsure<FloatConfigEntry>(key)->data;
	if (dst != value)
	{
		dst = value;
		m_dirty = true;
	}
}
void SkinConfig::Set(String key, const bool& value)
{
	bool& dst = SetEnsure<BoolConfigEntry>(key)->data;
	if (dst != value)
	{
		dst = value;
		m_dirty = true;
	}
}
void SkinConfig::Set(String key, const Color & value)
{
	Color& dst = SetEnsure<ColorConfigEntry>(key)->data;
	if (dst != value)
	{
		dst = value;
		m_dirty = true;
	}
}
void SkinConfig::Set(String key, const String& value)
{
	String& dst = SetEnsure<StringConfigEntry>(key)->data;
	if (dst != value)
	{
		dst = value;
		m_dirty = true;
	}
}


SettingsStyleConfig::SettingsStyleConfig()
{
	// Default state
	Clear();
	InitDefaults();
}

void SettingsStyleConfig::InitDefaults()
{
	Set(SettingsStyleConfigKeys::BackgroundColor, Colori(40,40,40));
	Set(SettingsStyleConfigKeys::BorderColor, Colori(0, 0, 0));
	Set(SettingsStyleConfigKeys::BorderWidth, 0);
	Set(SettingsStyleConfigKeys::TextColor, Colori(255, 255, 255));

	Set(SettingsStyleConfigKeys::ButtonBorderColor, Colori(0, 128, 255));
	Set(SettingsStyleConfigKeys::ButtonPaddingLeftRight, 5);
	Set(SettingsStyleConfigKeys::ButtonPaddingTopBottom, 5);
	Set(SettingsStyleConfigKeys::ButtonRounding, 0);
	Set(SettingsStyleConfigKeys::ButtonBorderWidth, 1);

	Set(SettingsStyleConfigKeys::ButtonBackgroundColor, Colori(40,40,40));
	Set(SettingsStyleConfigKeys::ButtonBackgroundColorHover, Colori(50, 50, 50));
	Set(SettingsStyleConfigKeys::ButtonTextColor, Colori(156, 156, 156));
	Set(SettingsStyleConfigKeys::ButtonTextColorHover, Colori(156, 156, 156));

	Set(SettingsStyleConfigKeys::NodeTextColor, Colori(156, 156, 156));
	Set(SettingsStyleConfigKeys::NodeArrowColor, Colori(175, 175, 175));
	Set(SettingsStyleConfigKeys::NodeArrowBackgroundColor, Colori(50, 50, 50));

	Set(SettingsStyleConfigKeys::OptionTextColor, Colori(156, 156, 156));
	Set(SettingsStyleConfigKeys::OptionColor, Colori(100,100,100));
	Set(SettingsStyleConfigKeys::OptionColorChecked, Colori(45, 45, 45));
	Set(SettingsStyleConfigKeys::OptionColorCheckedHover, Colori(45, 45, 45));
	Set(SettingsStyleConfigKeys::OptionColorHover, Colori(120, 120, 120));
	Set(SettingsStyleConfigKeys::OptionBorderColor, Colori(0,0,0,0));
	Set(SettingsStyleConfigKeys::OptionBorderWidth, 0);

	Set(SettingsStyleConfigKeys::CheckTextColor, Colori(156, 156, 156));
	Set(SettingsStyleConfigKeys::CheckColor, Colori(100, 100, 100));
	Set(SettingsStyleConfigKeys::CheckColorChecked, Colori(45, 45, 45));
	Set(SettingsStyleConfigKeys::CheckColorCheckedHover, Colori(45, 45, 45));
	Set(SettingsStyleConfigKeys::CheckColorHover, Colori(120, 120, 120));
	Set(SettingsStyleConfigKeys::CheckBorderColor, Colori(0, 0, 0, 0));
	Set(SettingsStyleConfigKeys::CheckBorderWidth, 0);

	Set(SettingsStyleConfigKeys::SliderBarFilledColor, Colori(120,120,120));
	Set(SettingsStyleConfigKeys::SliderBarEmptyColor, Colori(0,0,0));
	Set(SettingsStyleConfigKeys::SliderBallColor, Colori(120, 120, 120));
	Set(SettingsStyleConfigKeys::SliderBallColorHover, Colori(142, 142, 142));

	Set(SettingsStyleConfigKeys::ComboButtonTextColor, Colori(156, 156, 156));
	Set(SettingsStyleConfigKeys::ComboButtonTextColorHover, Colori(156, 156, 156));
	Set(SettingsStyleConfigKeys::ComboButtonBackgroundColor, Colori(50, 50, 50));
	Set(SettingsStyleConfigKeys::ComboButtonBackgroundColorHover, Colori(50, 50, 50));
	Set(SettingsStyleConfigKeys::ComboButtonArrowColor, Colori(120, 120, 120));

	Set(SettingsStyleConfigKeys::ComboBoxTextColor, Colori(156, 156, 156));
	Set(SettingsStyleConfigKeys::ComboBoxTextColorHover, Colori(156, 156, 156));
	Set(SettingsStyleConfigKeys::ComboBoxBackgroundColor, Colori(50, 50, 50));
	Set(SettingsStyleConfigKeys::ComboBoxBackgroundColorHover, Colori(60, 60, 60));

	Set(SettingsStyleConfigKeys::PropertyTextColor, Colori(156, 156, 156));
	Set(SettingsStyleConfigKeys::PropertyBackgroundColor, Colori(40, 40, 40));
	Set(SettingsStyleConfigKeys::PropertyEditTextColor, Colori(156, 156, 156));
	Set(SettingsStyleConfigKeys::PropertyEditBackgroundColor, Colori(40, 40, 40));
	Set(SettingsStyleConfigKeys::PropertyEditSelectedTextColor, Colori(40,40,40));
	Set(SettingsStyleConfigKeys::PropertyEditSelectedBackgroundColor, Colori(156, 156, 156));
	Set(SettingsStyleConfigKeys::PropertyArrowColor, Colori(156, 156, 156));

	Set(SettingsStyleConfigKeys::EditTextColor, Colori(156, 156, 156));
	Set(SettingsStyleConfigKeys::EditBackgroundColor, Colori(40, 40, 40));
	Set(SettingsStyleConfigKeys::EditSelectedTextColor, Colori(40,40,40));
	Set(SettingsStyleConfigKeys::EditSelectedBackgroundColor, Colori(156, 156, 156));


}