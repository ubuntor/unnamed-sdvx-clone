#include "stdafx.h"
#include "Application.hpp"
#include "GameConfig.hpp"
#include "Game.hpp"
#include "Track.hpp"
#include "LaserTrackBuilder.hpp"
#include <Beatmap/BeatmapPlayback.hpp>
#include <Beatmap/BeatmapObjects.hpp>
#include "AsyncAssetLoader.hpp"
#include <unordered_set>

const float Track::trackWidth = 1.0f;
const float Track::buttonWidth = 1.0f / 6;
const float Track::laserWidth = buttonWidth;
const float Track::fxbuttonWidth = buttonWidth * 2;
const float Track::buttonTrackWidth = buttonWidth * 4;

Track::Track()
{
	m_viewRange = 2.0f;
	if (g_aspectRatio < 1.0f)
		trackLength = 12.0f;
	else
		trackLength = 10.0f;
}
Track::~Track()
{
	if(loader)
		delete loader;

	for(uint32 i = 0; i < 2; i++)
	{
		if(m_laserTrackBuilder[i])
			delete m_laserTrackBuilder[i];
	}
	for(auto it = m_hitEffects.begin(); it != m_hitEffects.end(); it++)
	{
		delete *it;
	}

}
bool Track::AsyncLoad()
{
	loader = new AsyncAssetLoader();
	String skin = g_application->GetCurrentSkin();

	float laserHues[2] = { 0.f };
	laserHues[0] = g_gameConfig.GetFloat(GameConfigKeys::Laser0Color);
	laserHues[1] = g_gameConfig.GetFloat(GameConfigKeys::Laser1Color);
	m_btOverFxScale = Math::Clamp(g_gameConfig.GetFloat(GameConfigKeys::BTOverFXScale), 0.01f, 1.0f);

	for (uint32 i = 0; i < 2; i++)
		laserColors[i] = Color::FromHSV(laserHues[i],1.0,1.0);

	// Load hit effect colors
	Image hitColorPalette;
	CheckedLoad(hitColorPalette = ImageRes::Create(Path::Absolute("skins/" + skin + "/textures/hitcolors.png")));
	assert(hitColorPalette->GetSize().x >= 4);
	for(uint32 i = 0; i < 4; i++)
		hitColors[i] = hitColorPalette->GetBits()[i];

	// mip-mapped and anisotropicaly filtered track textures
	loader->AddTexture(trackTexture, "track.png");
	loader->AddTexture(trackTickTexture, "tick.png");

	// Scoring texture
	loader->AddTexture(scoreHitTexture, "scorehit.png");


	for(uint32 i = 0; i < 3; i++)
	{
		loader->AddTexture(scoreHitTextures[i], Utility::Sprintf("score%d.png", i));
	}

	// Load Button object
	loader->AddTexture(buttonTexture, "button.png");
	loader->AddTexture(buttonHoldTexture, "buttonhold.png");

	// Load FX object
	loader->AddTexture(fxbuttonTexture, "fxbutton.png");
	loader->AddTexture(fxbuttonHoldTexture, "fxbuttonhold.png");

	// Load Laser object
	loader->AddTexture(laserTextures[0], "laser_l.png");
	loader->AddTexture(laserTextures[1], "laser_r.png");

	// Entry and exit textures for laser
	loader->AddTexture(laserTailTextures[0], "laser_entry_l.png");
	loader->AddTexture(laserTailTextures[1], "laser_entry_r.png");
	loader->AddTexture(laserTailTextures[2], "laser_exit_l.png");
	loader->AddTexture(laserTailTextures[3], "laser_exit_r.png");

	// Track materials
	loader->AddMaterial(trackMaterial, "track");
	loader->AddMaterial(spriteMaterial, "sprite"); // General purpose material
	loader->AddMaterial(buttonMaterial, "button");
	loader->AddMaterial(holdButtonMaterial, "holdbutton");
	loader->AddMaterial(laserMaterial, "laser");
	loader->AddMaterial(baseBlackLaserMaterial, "blackLaser");

	loader->AddMaterial(trackOverlay, "overlay");

	return loader->Load();
}
bool Track::AsyncFinalize()
{
	// Finalizer loading textures/material/etc.
	bool success = loader->Finalize();
	delete loader;
	loader = nullptr;

	// Copy base materials to used materials
	for (size_t i = 0; i < 2; i++)
	{
		//TODO: Material copying, or something like that.
		for (size_t j = 0; j < 3; j++)
		{
			laserCurrentMaterial[i][j] = laserMaterial->Clone();
			laserMissedMaterial[i][j] = laserMaterial->Clone();
			laserComingMaterial[i][j] = laserMaterial->Clone();
		}
		blackLaserMaterial[i] = baseBlackLaserMaterial->Clone();
	}

	tickMaterial = buttonMaterial->Clone();

	// Load track cover material & texture here for skin back-compat
	trackCoverMaterial = g_application->LoadMaterial("trackCover");
	trackCoverTexture = g_application->LoadTexture("trackCover.png");
	if (trackCoverMaterial)
	{
		trackCoverMaterial->opaque = false;
	}

	// Set Texture states
	trackTexture->SetMipmaps(false);
	trackTexture->SetFilter(true, true, 16.0f);
	trackTexture->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
	trackTickTexture->SetMipmaps(true);
	trackTickTexture->SetFilter(true, true, 16.0f);
	trackTickTexture->SetWrap(TextureWrap::Repeat, TextureWrap::Clamp);
	trackTickLength = trackTickTexture->CalculateHeight(buttonTrackWidth);
	scoreHitTexture->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);

	buttonTexture->SetMipmaps(true);
	buttonTexture->SetFilter(true, true, 16.0f);
	buttonHoldTexture->SetMipmaps(true);
	buttonHoldTexture->SetFilter(true, true, 16.0f);
	buttonLength = buttonTexture->CalculateHeight(buttonWidth);
	buttonMesh = MeshGenerators::Quad(g_gl, Vector2(0.0f, 0.0f), Vector2(buttonWidth, buttonLength));
	buttonMaterial->opaque = false;

	fxbuttonTexture->SetMipmaps(true);
	fxbuttonTexture->SetFilter(true, true, 16.0f);
	fxbuttonHoldTexture->SetMipmaps(true);
	fxbuttonHoldTexture->SetFilter(true, true, 16.0f);
	fxbuttonLength = fxbuttonTexture->CalculateHeight(fxbuttonWidth);
	fxbuttonMesh = MeshGenerators::Quad(g_gl, Vector2(0.0f, 0.0f), Vector2(fxbuttonWidth, fxbuttonLength));

	holdButtonMaterial->opaque = false;

	for (uint32 i = 0; i < 2; i++)
	{
		laserTextures[i]->SetMipmaps(true);
		laserTextures[i]->SetFilter(true, true, 16.0f);
		laserTextures[i]->SetWrap(TextureWrap::Clamp, TextureWrap::Repeat);
	}

	for(uint32 i = 0; i < 4; i++)
	{
		laserTailTextures[i]->SetMipmaps(true);
		laserTailTextures[i]->SetFilter(true, true, 16.0f);
		laserTailTextures[i]->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
	}

	// Track and sprite material (all transparent)
	trackMaterial->opaque = false;
	trackMaterial->params.SetParameter("mainTex", trackTexture);
	trackMaterial->params.SetParameter("lCol", laserColors[0]);
	trackMaterial->params.SetParameter("rCol", laserColors[1]);
	spriteMaterial->opaque = false;

	for (size_t i = 0; i < 6; i++)
	{
		btHitMaterial[i] = spriteMaterial->Clone();
		btHitRatingMaterial[i] = spriteMaterial->Clone();
	}

	// Laser object material, allows coloring and sampling laser edge texture
	for (size_t i = 0; i < 2; i++)
	{
		for (size_t j = 0; j < 3; j++)
		{
			Texture t;
			if (j == 0)
				t = laserTailTextures[i];
			else if (j == 1)
				t = laserTextures[i];
			else
				t = laserTailTextures[i + 2];

			laserCurrentMaterial[i][j]->blendMode = MaterialBlendMode::Additive;
			laserCurrentMaterial[i][j]->opaque = false;
			laserComingMaterial[i][j]->blendMode = MaterialBlendMode::Additive;
			laserComingMaterial[i][j]->opaque = false;
			laserMissedMaterial[i][j]->blendMode = MaterialBlendMode::Additive;
			laserMissedMaterial[i][j]->opaque = false;

			laserCurrentMaterial[i][j]->params.SetParameter("mainTex", t);
			laserComingMaterial[i][j]->params.SetParameter("mainTex", t);
			laserMissedMaterial[i][j]->params.SetParameter("mainTex", t);

			laserCurrentMaterial[i][j]->params.SetParameter("color", laserColors[i]);
			laserComingMaterial[i][j]->params.SetParameter("color", laserColors[i]);
			laserMissedMaterial[i][j]->params.SetParameter("color", laserColors[i]);


			laserMissedMaterial[i][j]->params.SetParameter("objectGlow", 0.3f);
			laserComingMaterial[i][j]->params.SetParameter("objectGlow", 0.75f);

		}
		blackLaserMaterial[i]->opaque = false;
		blackLaserMaterial[i]->params.SetParameter("mainTex", laserTextures[i]);
	}


	tickMaterial->params.SetParameter("mainTex", trackTickTexture);
	tickMaterial->params.SetParameter("hasSample", false);

	// Overlay shader
	trackOverlay->opaque = false;

	// Create a laser track builder for each laser object
	// these will output and cache meshes for rendering lasers
	for(uint32 i = 0; i < 2; i++)
	{
		m_laserTrackBuilder[i] = new LaserTrackBuilder(g_gl, this, i);
		m_laserTrackBuilder[i]->laserBorderPixels = 12;
		m_laserTrackBuilder[i]->laserLengthScale = trackLength / (GetViewRange() * laserSpeedOffset);
		m_laserTrackBuilder[i]->Reset(); // Also initializes the track builder
	}

	// Generate simple planes for the playfield track and elements
	trackMesh = MeshGenerators::Quad(g_gl, Vector2(-trackWidth * 0.5f, -1), Vector2(trackWidth, trackLength + 1));
	
	for (size_t i = 0; i < 2; i++)
	{
		//track base
		Vector2 pos = Vector2(-trackWidth * 0.5f * i, -1);
		Vector2 size = Vector2(trackWidth / 2.0f, trackLength + 1);
		Rect rect = Rect(pos, size);
		Rect uv = Rect(0.5 - 0.5 * i, 0.0f, 1.0 - 0.5 * i, 1.0f);
		splitTrackMesh[i] = MeshRes::Create(g_gl);
		splitTrackMesh[i]->SetPrimitiveType(PrimitiveType::TriangleList);
		Vector<MeshGenerators::SimpleVertex> splitMeshData;
		MeshGenerators::GenerateSimpleXYQuad(rect, uv, splitMeshData);
		splitTrackMesh[i]->SetData(splitMeshData);

		//track cover
		pos = Vector2(-trackWidth * 0.5f * i, -trackLength);
		size = Vector2(trackWidth / 2.0f, trackLength * 2.0);
		rect = Rect(pos, size);
		splitTrackCoverMesh[i] = MeshRes::Create(g_gl);
		splitTrackCoverMesh[i]->SetPrimitiveType(PrimitiveType::TriangleList);
		splitMeshData.clear();
		MeshGenerators::GenerateSimpleXYQuad(rect, uv, splitMeshData);
		splitTrackCoverMesh[i]->SetData(splitMeshData);

		//tick meshes
		pos = Vector2(-buttonTrackWidth * 0.5f * i, 0.0f);
		size = Vector2(buttonTrackWidth / 2.0f, trackTickLength);
		rect = Rect(pos, size);
		splitTrackTickMesh[i] = MeshRes::Create(g_gl);
		splitTrackTickMesh[i]->SetPrimitiveType(PrimitiveType::TriangleList);
		splitMeshData.clear();
		MeshGenerators::GenerateSimpleXYQuad(rect, uv, splitMeshData);
		splitTrackTickMesh[i]->SetData(splitMeshData);
	}
	
	calibrationCritMesh = MeshGenerators::Quad(g_gl, Vector2(-trackWidth * 0.5f, -0.02f), Vector2(trackWidth, 0.02f));
	calibrationDarkMesh = MeshGenerators::Quad(g_gl, Vector2(-trackWidth * 0.5f, -1.0f), Vector2(trackWidth, 0.99f));
	trackCoverMesh = MeshGenerators::Quad(g_gl, Vector2(-trackWidth * 0.5f, -trackLength), Vector2(trackWidth, trackLength * 2));
	trackTickMesh = MeshGenerators::Quad(g_gl, Vector2(-buttonTrackWidth * 0.5f, 0.0f), Vector2(buttonTrackWidth, trackTickLength));
	centeredTrackMesh = MeshGenerators::Quad(g_gl, Vector2(-0.5f, -0.5f), Vector2(1.0f, 1.0f));
	uint8 whiteData[4] = { 255, 255, 255, 255 };
	whiteTexture = TextureRes::Create(g_gl);
	whiteTexture->SetData({ 1,1 }, (void*)whiteData);


	return success;
}
void Track::Tick(class BeatmapPlayback& playback, float deltaTime)
{
	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();
	if(&currentTimingPoint != m_lastTimingPoint)
	{
		m_lastTimingPoint = &currentTimingPoint;
	}

	// Calculate track origin transform
	uint8 portrait = g_aspectRatio > 1.0f ? 0 : 1;

	// Button Hit FX
	for(auto it = m_hitEffects.begin(); it != m_hitEffects.end();)
	{
		(*it)->Tick(deltaTime);
		if((*it)->time <= 0.0f)
		{
			delete *it;
			it = m_hitEffects.erase(it);
			continue;
		}
		it++;
	}

	MapTime currentTime = playback.GetLastTime();

	// Set the view range of the track
	trackViewRange = Vector2((float)currentTime, 0.0f);
	trackViewRange.y = trackViewRange.x + GetViewRange();

	// Update ticks separating bars to draw
	double tickTime = (double)currentTime;
	MapTime rangeEnd = currentTime + playback.ViewDistanceToDuration(m_viewRange);
	const TimingPoint* tp = playback.GetTimingPointAt((MapTime)tickTime);
	double stepTime = tp->GetBarDuration(); // Every xth note based on signature

	// Overflow on first tick
	double firstOverflow = fmod((double)tickTime - tp->time, stepTime);
	if(fabs(firstOverflow) > 1)
		tickTime -= firstOverflow;

	m_barTicks.clear();

	// Add first tick
	m_barTicks.Add(playback.TimeToViewDistance((MapTime)tickTime));

	while(tickTime < rangeEnd)
	{
		double next = tickTime + stepTime;

		const TimingPoint* tpNext = playback.GetTimingPointAt((MapTime)tickTime);
		if(tpNext != tp)
		{
			tp = tpNext;
			tickTime = tp->time;
			stepTime = tp->GetBarDuration(); // Every xth note based on signature
		}
		else
		{
			tickTime = next;
		}

		// Add tick
		m_barTicks.Add(playback.TimeToViewDistance((MapTime)tickTime));
	}

	// Update track hide status
	m_trackHide += m_trackHideSpeed * deltaTime;
	m_trackHide = Math::Clamp(m_trackHide, 0.0f, 1.0f);

	// Set Object glow
	int32 startBeat = 0;
	uint32 numBeats = playback.CountBeats(m_lastMapTime, currentTime - m_lastMapTime, startBeat, 4);
	objectGlowState = currentTime % 100 < 50 ? 0 : 1;
	m_lastMapTime = currentTime;

	objectGlow = fabs((currentTime % 100) / 50.0 - 1) * 0.5 + 0.5;

	/*
	if(numBeats > 0)
	{
		objectGlow = 1.0f;
	}
	else
	{
		objectGlow -= 7.0f * deltaTime;
		if(objectGlow < 0.0f)
			objectGlow = 0.0f;
	}
	*/

	// Perform laser track cache cleanup, etc.
	for(uint32 i = 0; i < 2; i++)
	{
		m_laserTrackBuilder[i]->Update(m_lastMapTime);

		//laserAlertOpacity[i] = (-pow(m_alertTimer[i], 2.0f) + (1.5f * m_alertTimer[i])) * 5.0f;
		//laserAlertOpacity[i] = Math::Clamp<float>(laserAlertOpacity[i], 0.0f, 1.0f);
		//m_alertTimer[i] += deltaTime;
	}

	//set material parameters



	const auto setCommonParams = [&](Material& mat) {
		//TODO: needs to be figured out
		mat->params.SetParameter("trackPos", 0.5f);
		mat->params.SetParameter("trackScale", 1.0f / trackLength);
		mat->params.SetParameter("hiddenCutoff", hiddenCutoff); // Hidden cutoff (% of track)
		mat->params.SetParameter("hiddenFadeWindow", hiddenFadewindow); // Hidden cutoff (% of track)
		mat->params.SetParameter("suddenCutoff", suddenCutoff); // Hidden cutoff (% of track)
		mat->params.SetParameter("suddenFadeWindow", suddenFadewindow); // Hidden cutoff (% of track)
	};

	for (size_t i = 0; i < 2; i++)
	{
		for (size_t j = 0; j < 3; j++)
		{
			setCommonParams(laserComingMaterial[i][j]);
			setCommonParams(laserCurrentMaterial[i][j]);
			laserCurrentMaterial[i][j]->params.SetParameter("objectGlow", objectGlow);
		}
	}

	trackMaterial->params.SetParameter("hidden", m_trackHide);
	
}

void Track::DrawLaserBase(RenderQueue& rq, class BeatmapPlayback& playback, const Vector<ObjectState*>& objects)
{
	for (auto obj : objects)
	{
		if (obj->type != ObjectType::Laser)
			continue;

		LaserObjectState* laser = (LaserObjectState*)obj;
		if ((laser->flags & LaserObjectState::flag_Extended) != 0 || m_trackHide > 0.f)
		{
			// Calculate height based on time on current track
			float viewRange = GetViewRange();
			float position = playback.TimeToViewDistance(obj->time);
			float posmult = trackLength / (m_viewRange * laserSpeedOffset);

			Mesh laserMesh = m_laserTrackBuilder[laser->index]->GenerateTrackMesh(playback, laser);

			// Get the length of this laser segment
			Transform laserTransform = trackOrigin;
			laserTransform *= Transform::Translation(Vector3{ 0.0f, posmult * position, 0.0f });

			if (laserMesh)
			{
				rq.Draw(laserTransform, laserMesh, blackLaserMaterial[laser->index]);
			}
		}
	}
}

void Track::DrawBase(class RenderQueue& rq)
{
	// Base
	Transform transform = trackOrigin;
	if (centerSplit != 0.0f)
	{
		rq.Draw(transform * Transform::Translation({centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[0], trackMaterial);
		rq.Draw(transform * Transform::Translation({-centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[1], trackMaterial);
	}
	else
	{
		rq.Draw(transform, trackMesh, trackMaterial);
	}

	for (float f : m_barTicks)
	{
		float fLocal = f / m_viewRange;
		Vector3 tickPosition = Vector3(0.0f, trackLength * fLocal - trackTickLength * 0.5f, 0.01f);
		Transform tickTransform = trackOrigin;
		tickTransform *= Transform::Translation(tickPosition);
		if (centerSplit != 0.0f)
		{
			rq.Draw(tickTransform * Transform::Translation({ centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f }), splitTrackTickMesh[0], tickMaterial);
			rq.Draw(tickTransform * Transform::Translation({ -centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f }), splitTrackTickMesh[1], tickMaterial);
		}
		else
		{
			rq.Draw(tickTransform, trackTickMesh, tickMaterial);
		}
	}
}
void Track::DrawObjectState(RenderQueue& rq, class BeatmapPlayback& playback, ObjectState* obj, bool active, const std::unordered_set<MapTime> chipFXTimes[2])
{
	// Calculate height based on time on current track
	float viewRange = GetViewRange();
	float position = playback.TimeToViewDistance(obj->time) / viewRange;
	float glow = 0.0f;

	if(obj->type == ObjectType::Single || obj->type == ObjectType::Hold)
	{
		bool isHold = obj->type == ObjectType::Hold;
		MultiObjectState* mobj = (MultiObjectState*)obj;
		Material mat = buttonMaterial;
		Mesh mesh;
		float xscale = 1.0f;
		float width;
		float xposition;
		float length;
		float currentObjectGlow = active ? objectGlow : 0.3f;
		int currentObjectGlowState = active ? 2 + objectGlowState : 0;
		if(mobj->button.index < 4) // Normal button
		{
			width = buttonWidth;
			xposition = buttonTrackWidth * -0.5f + width * mobj->button.index;
			int fxIdx = 0;
			if (mobj->button.index < 2)
			{
				xposition -= 0.5 * centerSplit * buttonWidth;
			}
			else 
			{
				xposition += 0.5 * centerSplit * buttonWidth;
				fxIdx = 1;
			}
			if (!isHold && chipFXTimes[fxIdx].count(mobj->time))
			{
				xscale = m_btOverFxScale;
				xposition += width * ((1.0 - xscale) / 2.0);
			}
			length = buttonLength;
			//params.SetParameter("hasSample", mobj->button.hasSample);
			//params.SetParameter("mainTex", isHold ? buttonHoldTexture : buttonTexture);
			mesh = buttonMesh;
		}
		else // FX Button
		{
			width = fxbuttonWidth;
			xposition = buttonTrackWidth * -0.5f + fxbuttonWidth *(mobj->button.index - 4);
			if (mobj->button.index < 5)
			{
				xposition -= 0.5f * centerSplit * buttonWidth;
			}
			else
			{
				xposition += 0.5f * centerSplit * buttonWidth;
			}
			length = fxbuttonLength;
			mesh = fxbuttonMesh;
		}

		//params.SetParameter("trackPos", position);

		if(isHold)
		{
			/*if(!active && mobj->hold.GetRoot()->time > playback.GetLastTime())
				params.SetParameter("hitState", 1);
			else
				params.SetParameter("hitState", currentObjectGlowState);

			params.SetParameter("objectGlow", currentObjectGlow);*/
			mat = holdButtonMaterial;
		}

		Vector3 buttonPos = Vector3(xposition, trackLength * position, 0.0f);

		Transform buttonTransform = trackOrigin;
		buttonTransform *= Transform::Translation(buttonPos);
		float scale = 1.0f;
		if(isHold) // Hold Note?
		{
			float trackScale = (playback.DurationToViewDistanceAtTime(mobj->time, mobj->hold.duration) / viewRange) / length;
			scale = trackScale * trackLength;

			//params.SetParameter("trackScale", trackScale);
		}
		else {
			//Use actual distance from camera instead of position on the track?
			scale = 1.0f + (Math::Max(1.0f, distantButtonScale) - 1.0f) * position;
			//params.SetParameter("trackScale", 1.0f / trackLength);
		}

		//params.SetParameter("hiddenCutoff", hiddenCutoff); // Hidden cutoff (% of track)
		//params.SetParameter("hiddenFadeWindow", hiddenFadewindow); // Hidden cutoff (% of track)
		//params.SetParameter("suddenCutoff", suddenCutoff); // Sudden cutoff (% of track)
		//params.SetParameter("suddenFadeWindow", suddenFadewindow); // Sudden cutoff (% of track)


		buttonTransform *= Transform::Scale({ xscale, scale, 1.0f });
		rq.Draw(buttonTransform, mesh, mat);
	}
	else if(obj->type == ObjectType::Laser) // Draw laser
	{
		

		position = playback.TimeToViewDistance(obj->time);
		float posmult = trackLength / (m_viewRange * laserSpeedOffset);
		LaserObjectState* laser = (LaserObjectState*)obj;

		// Draw segment function
		auto DrawSegment = [&](Mesh mesh, int section)
		{
			assert(section < 3);
			Material mat = laserMissedMaterial[laser->index][section];
			if (laser->GetRoot()->time > playback.GetLastTime())
				mat = laserComingMaterial[laser->index][section];
			else if(active)
				mat = laserCurrentMaterial[laser->index][section];


			// Get the length of this laser segment
			Transform laserTransform = trackOrigin;
			laserTransform *= Transform::Translation(Vector3{ 0.0f, posmult * position,
				0.0f });

			if(mesh)
			{
				rq.Draw(laserTransform, mesh, mat);
			}
		};

		// Draw entry?
		if(!laser->prev)
		{
			Mesh laserTail = m_laserTrackBuilder[laser->index]->GenerateTrackEntry(playback, laser);
			DrawSegment(laserTail, 0);
		}

		// Body
		Mesh laserMesh = m_laserTrackBuilder[laser->index]->GenerateTrackMesh(playback, laser);
		DrawSegment(laserMesh, 1);

		// Draw exit?
		if(!laser->next && (laser->flags & LaserObjectState::flag_Instant) != 0) // Only draw exit on slams
		{
			Mesh laserTail = m_laserTrackBuilder[laser->index]->GenerateTrackExit(playback, laser);
			DrawSegment(laserTail, 2);
		}
	}
}
void Track::DrawOverlays(class RenderQueue& rq)
{
	// Draw button hit effect sprites
	for(auto& hfx : m_hitEffects)
	{
		hfx->Draw(rq);
	}
}
void Track::DrawTrackOverlay(RenderQueue& rq, float heightOffset /*= 0.05f*/, float widthScale /*= 1.0f*/)
{
	Transform transform = trackOrigin;
	transform *= Transform::Scale({ widthScale, 1.0f, 1.0f });
	transform *= Transform::Translation({ 0.0f, heightOffset, 0.0f });
	rq.Draw(transform, trackMesh, trackOverlay);
}
void Track::DrawSprite(RenderQueue& rq, Vector3 pos, Vector2 size, Material material, float tilt /*= 0.0f*/)
{
	Transform spriteTransform = trackOrigin;
	spriteTransform *= Transform::Translation(pos);
	spriteTransform *= Transform::Scale({ size.x, size.y, 1.0f });
	if(tilt != 0.0f)
		spriteTransform *= Transform::Rotation({ tilt, 0.0f, 0.0f });

	rq.Draw(spriteTransform, centeredTrackMesh, material);
}

void Track::DrawTrackCover(RenderQueue& rq)
{
	#ifndef EMBEDDED
	if (trackCoverMaterial && trackCoverTexture)
	{
		Transform t = trackOrigin;
		trackCoverMaterial->params.SetParameter("mainTex", trackCoverTexture);
		trackCoverMaterial->params.SetParameter("hiddenCutoff", hiddenCutoff); // Hidden cutoff (% of track)
		trackCoverMaterial->params.SetParameter("hiddenFadeWindow", hiddenFadewindow); // Hidden cutoff (% of track)
		trackCoverMaterial->params.SetParameter("suddenCutoff", suddenCutoff); // Hidden cutoff (% of track)
		trackCoverMaterial->params.SetParameter("suddenFadeWindow", suddenFadewindow); // Hidden cutoff (% of track)

		if (centerSplit != 0.0f)
		{
			rq.Draw(t * Transform::Translation({ centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f }), splitTrackCoverMesh[0], trackCoverMaterial);
			rq.Draw(t * Transform::Translation({ -centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f }), splitTrackCoverMesh[1], trackCoverMaterial);
		}
		else
		{
			rq.Draw(t, trackCoverMesh, trackCoverMaterial);
		}
	}
	#endif
}

void Track::DrawCalibrationCritLine(RenderQueue& rq, Material lineMat, Material darkMat)
{
	Transform t = trackOrigin;
	rq.Draw(t, calibrationDarkMesh, darkMat);
	rq.Draw(t, calibrationCritMesh, lineMat);
}

Vector3 Track::TransformPoint(const Vector3 & p)
{
	return trackOrigin.TransformPoint(p);
}

TimedEffect* Track::AddEffect(TimedEffect* effect)
{
	m_hitEffects.Add(effect);
	effect->track = this;
	return effect;
}
void Track::ClearEffects()
{
	m_trackHide = 0.0f;
	m_trackHideSpeed = 0.0f;

	for(auto it = m_hitEffects.begin(); it != m_hitEffects.end(); it++)
	{
		delete *it;
	}
	m_hitEffects.clear();
}

void Track::SetViewRange(float newRange)
{
	if(newRange != m_viewRange)
	{
		m_viewRange = newRange;

		// Update view range
		float newLaserLengthScale = trackLength / (m_viewRange * laserSpeedOffset);
		m_laserTrackBuilder[0]->laserLengthScale = newLaserLengthScale;
		m_laserTrackBuilder[1]->laserLengthScale = newLaserLengthScale;

		// Reset laser tracks cause these won't be correct anymore
		m_laserTrackBuilder[0]->Reset();
		m_laserTrackBuilder[1]->Reset();
	}
}

void Track::SendLaserAlert(uint8 laserIdx)
{
	if (m_alertTimer[laserIdx] > 3.0f)
		m_alertTimer[laserIdx] = 0.0f;
}

void Track::SetLaneHide(bool hide, double duration)
{
	m_trackHideSpeed = hide ? 1.0f / duration : -1.0f / duration;
}

float Track::GetViewRange() const
{
	return m_viewRange;
}

float Track::GetButtonPlacement(uint32 buttonIdx)
{
	if (buttonIdx < 4)
	{
		float x = buttonIdx * buttonWidth - (buttonWidth * 1.5f);
		if (buttonIdx < 2)
		{
			x -= 0.5 * centerSplit * buttonWidth;
		}
		else
		{
			x += 0.5 * centerSplit * buttonWidth;
		}
		return x;
	}
	else
	{
		float x = (buttonIdx - 4) * fxbuttonWidth - (fxbuttonWidth * 0.5f);
		if (buttonIdx < 5)
		{
			x -= 0.5 * centerSplit * buttonWidth;
		}
		else
		{
			x += 0.5 * centerSplit * buttonWidth;
		}
		return x;
	}
}

