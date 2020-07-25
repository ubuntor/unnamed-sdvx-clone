#include "stdafx.h"
#include "RenderQueue.hpp"
#include "OpenGL.hpp"
using Utility::Cast;

namespace Graphics
{
	RenderQueue::RenderQueue(OpenGL* ogl, const RenderState& rs)
	{
		m_ogl = ogl;
		m_renderState = rs;
	}
	RenderQueue::RenderQueue(RenderQueue&& other)
	{
		m_ogl = other.m_ogl;
		other.m_ogl = nullptr;
		m_orderedCommands = move(other.m_orderedCommands);
		m_renderState = other.m_renderState;
	}
	RenderQueue& RenderQueue::operator=(RenderQueue&& other)
	{
		Clear();
		m_ogl = other.m_ogl;
		other.m_ogl = nullptr;
		m_orderedCommands = move(other.m_orderedCommands);
		m_renderState = other.m_renderState;
		return *this;
	}
	RenderQueue::~RenderQueue()
	{
		Clear();
	}
	void RenderQueue::Process(bool clearQueue)
	{
		assert(m_ogl);

		bool scissorEnabled = false;
		bool blendEnabled = false;
		MaterialBlendMode activeBlendMode = (MaterialBlendMode)-1;

		Set<Material> initializedShaders;
		Mesh currentMesh;
		Material currentMaterial;

		// Create a new list of items
		for(RenderQueueItem* item : m_orderedCommands)
		{
			auto SetupMaterial = [&](Material mat)
			{
				// Only bind params if material is already bound to context
				if(currentMaterial == mat)
					mat->BindParameters(m_renderState.worldTransform);
				else
				{
					if(initializedShaders.Contains(mat))
					{
						// Only bind params and rebind
						mat->BindParameters(m_renderState.worldTransform);
						mat->BindToContext();
						currentMaterial = mat;
					}
					else
					{
						mat->Bind(m_renderState);
						initializedShaders.Add(mat);
						currentMaterial = mat;
					}
				}

				// Setup Render state for transparent object
				if(mat->opaque)
				{
					if(blendEnabled)
					{
						glDisable(GL_BLEND);
						blendEnabled = false;
					}
				}
				else
				{
					if(!blendEnabled)
					{
						glEnable(GL_BLEND);
						blendEnabled = true;
					}
					if(activeBlendMode != mat->blendMode)
					{
						switch(mat->blendMode)
						{
						case MaterialBlendMode::Normal:
							glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
							break;
						case MaterialBlendMode::Additive:
							glBlendFunc(GL_ONE, GL_ONE);
							break;
						case MaterialBlendMode::Multiply:
							glBlendFunc(GL_SRC_ALPHA, GL_SRC_COLOR);
							break;
						}
					}
				}
			};

			// Draw mesh helper
			auto DrawOrRedrawMesh = [&](Mesh mesh)
			{
				if(currentMesh == mesh)
					mesh->Redraw();
				else
				{
					mesh->Draw();
					currentMesh = mesh;
				}
			};

			if(Cast<SimpleDrawCall>(item))
			{
				SimpleDrawCall* sdc = (SimpleDrawCall*)item;
				m_renderState.worldTransform = sdc->worldTransform;
				SetupMaterial(sdc->mat);

				// Check if scissor is enabled
				bool useScissor = (sdc->scissorRect.size.x >= 0);
				if(useScissor)
				{
					// Apply scissor
					if(!scissorEnabled)
					{
						glEnable(GL_SCISSOR_TEST);
						scissorEnabled = true;
					}
					float scissorY = m_renderState.viewportSize.y - sdc->scissorRect.Bottom();
					glScissor((int32)sdc->scissorRect.Left(), (int32)scissorY,
						(int32)sdc->scissorRect.size.x, (int32)sdc->scissorRect.size.y);
				}
				else
				{
					if(scissorEnabled)
					{
						glDisable(GL_SCISSOR_TEST);
						scissorEnabled = false;
					}
				}

				DrawOrRedrawMesh(sdc->mesh);
				#ifdef EMBEDDED
				glUseProgram(0);
				#endif
			}
			else if(Cast<PointDrawCall>(item))
			{
				if(scissorEnabled)
				{
					// Disable scissor
					glDisable(GL_SCISSOR_TEST);
					scissorEnabled = false;
				}

				PointDrawCall* pdc = (PointDrawCall*)item;
				m_renderState.worldTransform = Transform();
				SetupMaterial(pdc->mat);
				PrimitiveType pt = pdc->mesh->GetPrimitiveType();
				if(pt >= PrimitiveType::LineList && pt <= PrimitiveType::LineStrip)
				{
					glLineWidth(pdc->size);
				}
				else
				{
					#ifndef EMBEDDED
					glPointSize(pdc->size);
					#endif
				}
				
				DrawOrRedrawMesh(pdc->mesh);
				#ifdef EMBEDDED
				glUseProgram(0);
				#endif
			}
			else if (Cast<TextDrawCall>(item))
			{
				TextDrawCall* tdc = (TextDrawCall*)item;
				m_renderState.worldTransform = tdc->worldTransform;
				tdc->text->GetMaterial()->params.SetParameter("mainTex", tdc->text->GetTexture());
				tdc->text->GetMaterial()->params.SetParameter("color", tdc->color);
				SetupMaterial(tdc->text->GetMaterial());

				// Check if scissor is enabled
				bool useScissor = (tdc->scissorRect.size.x >= 0);
				if (useScissor)
				{
					// Apply scissor
					if (!scissorEnabled)
					{
						glEnable(GL_SCISSOR_TEST);
						scissorEnabled = true;
					}
					float scissorY = m_renderState.viewportSize.y - tdc->scissorRect.Bottom();
					glScissor((int32)tdc->scissorRect.Left(), (int32)scissorY,
						(int32)tdc->scissorRect.size.x, (int32)tdc->scissorRect.size.y);
				}
				else
				{
					if (scissorEnabled)
					{
						glDisable(GL_SCISSOR_TEST);
						scissorEnabled = false;
					}
				}


				DrawOrRedrawMesh(tdc->text->GetMesh());
#ifdef EMBEDDED
				glUseProgram(0);
#endif
			}
		}

		// Disable all states that were on
		glDisable(GL_BLEND);
		glDisable(GL_SCISSOR_TEST);

		if(clearQueue)
		{
			Clear();
		}
	}

	void RenderQueue::Clear()
	{
		// Cleanup the list of items
		for(RenderQueueItem* item : m_orderedCommands)
		{
			delete item;
		}
		m_orderedCommands.clear();
	}

	void RenderQueue::Draw(Transform worldTransform, Mesh m, Material mat)
	{
		SimpleDrawCall* sdc = new SimpleDrawCall();
		sdc->mat = mat;
		sdc->mesh = m;
		sdc->worldTransform = worldTransform;
		m_orderedCommands.push_back(sdc);
	}
	void RenderQueue::DrawText(Transform worldTransform, Ref<class TextRes> text, Color c)
	{
		TextDrawCall* tdc = new TextDrawCall();
		tdc->text = text;
		tdc->color = c;
		tdc->worldTransform = worldTransform;
		m_orderedCommands.push_back(tdc);
	}

	void RenderQueue::DrawScissored(Rect scissor, Transform worldTransform, Mesh m, Material mat)
	{
		SimpleDrawCall* sdc = new SimpleDrawCall();
		sdc->mat = mat;
		sdc->mesh = m;
		sdc->worldTransform = worldTransform;
		sdc->scissorRect = scissor;
		m_orderedCommands.push_back(sdc);
	}
	void RenderQueue::DrawScissoredText(Rect scissor, Transform worldTransform, Ref<class TextRes> text, Color c)
	{
		TextDrawCall* tdc = new TextDrawCall();
		tdc->text = text;
		tdc->color = c;
		tdc->worldTransform = worldTransform;
		tdc->scissorRect = scissor;
		m_orderedCommands.push_back(tdc);
	}

	void RenderQueue::DrawPoints(Mesh m, Material mat, float pointSize)
	{
		PointDrawCall* pdc = new PointDrawCall();
		pdc->mat = mat;
		pdc->mesh = m;
		pdc->size = pointSize;
		m_orderedCommands.push_back(pdc);
	}

	// Initializes the simple draw call structure
	SimpleDrawCall::SimpleDrawCall()
		: scissorRect(Vector2(), Vector2(-1))
	{
	}

}
