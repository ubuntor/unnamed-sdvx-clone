#pragma once
#include <Graphics/ResourceTypes.hpp>

#ifdef None
#undef None
#endif

namespace Graphics
{
	/*
		A prerendered text object, contains all the vertices and texture sheets to draw itself
	*/
	class TextRes
	{
		friend class Font_Impl;
		struct FontSize* fontSize;
		Ref<class MeshRes> mesh;
		Ref<class MaterialRes> material;
	public:
		~TextRes();
		Ref<class MaterialRes> GetMaterial();
		Ref<class TextureRes> GetTexture();
		Ref<class MeshRes> GetMesh() { return mesh; }
		void Draw();
		Vector2 size;
	};

	/*
		Font class, can create Text objects
	*/
	class FontRes
	{
	public:
		virtual ~FontRes() = default;
		static Ref<FontRes> Create(class OpenGL* gl, const String& assetPath);
		static bool InitLibrary();
		static void FreeLibrary();
	public:
		// Text rendering options
		enum TextOptions
		{
			None = 0,
			Monospace = 0x1,
		};

		// Renders the input string into a drawable text object
		virtual Ref<TextRes> CreateText(const WString& str, uint32 nFontSize, Ref<class MaterialRes> textMaterial, TextOptions options = TextOptions::None) = 0;

	private:
		static bool LoadFallbackFont();
	};

	typedef Ref<FontRes> Font;
	typedef Ref<TextRes> Text;

	DEFINE_RESOURCE_TYPE(Font, FontRes)
}
