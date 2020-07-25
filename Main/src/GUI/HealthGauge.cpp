#include "stdafx.h"
#include "HealthGauge.hpp"
#include "Application.hpp"

HealthGauge::HealthGauge()
{
}

void HealthGauge::SetParams()
{
	backMaterial->params.SetParameter("color", Color::White);
	backMaterial->params.SetParameter("mainTex", backTexture);
	fillMaterial->params.SetParameter("mainTex", fillTexture);
	fillMaterial->params.SetParameter("maskTex", maskTexture);
	frontMaterial->params.SetParameter("color", Color::White);
	frontMaterial->params.SetParameter("mainTex", frontTexture);
}

void HealthGauge::Render(Mesh m, float deltaTime, Transform t)
{
	Color color;
	if(rate >= colorBorder)
	{
		color = upperColor;
	}
	else
	{
		color = lowerColor;
	}

	if(backTexture)
		g_application->GetRenderQueueBase()->Draw(t, m, backMaterial);

	fillMaterial->params.SetParameter("rate", rate);
	fillMaterial->params.SetParameter("barColor", color);
	g_application->GetRenderQueueBase()->Draw(t, m, fillMaterial);

	g_application->GetRenderQueueBase()->Draw(t, m, frontMaterial);
}

Vector2 HealthGauge::GetDesiredSize()
{
	//return GUISlotBase::ApplyFill(FillMode::Fit, frontTexture->GetSize(), rd.area).size;
	return Vector2();
}
