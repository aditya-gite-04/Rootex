#pragma once

#include "components/visual/render_ui_component.h"
#include "resource_loader.h"

/// Component to render 2D UI Text
class TextUIComponent : public RenderUIComponent
{
public:
	/// DirectXTK flipping modes for sprites
	enum class Mode
	{
		None = DirectX::SpriteEffects_None,
		FlipX = DirectX::SpriteEffects_FlipHorizontally,
		FlipY = DirectX::SpriteEffects_FlipVertically,
		FlipXY = DirectX::SpriteEffects_FlipBoth,
	};

	static Component* Create(const JSON::json& componentData);
	static Component* CreateDefault();

	/// Font file
	FontResourceFile* m_FontFile;
	/// Text to display
	String m_Text;
	/// Color of text
	Color m_Color;
	/// Flipping mode
	Mode m_Mode;
	/// 2D origin of the Font
	Vector2 m_Origin;

	TextUIComponent(FontResourceFile* font, const String& text, const Color& color, const Mode& mode, const Vector2& origin, const bool& isVisible);
	TextUIComponent(TextUIComponent&) = delete;
	virtual ~TextUIComponent() = default;

	friend class EntityFactory;

public:
	static void RegisterAPI(sol::table& rootex);
	static const ComponentID s_ID = (ComponentID)ComponentIDs::TextUIComponent;

	virtual void render() override;

	virtual ComponentID getComponentID() const override { return s_ID; }
	virtual String getName() const override { return "TextUIComponent"; };
	virtual JSON::json getJSON() const override;

	void setFont(FontResourceFile* fontFile) { m_FontFile = fontFile; }
	void setText(const String& text) { m_Text = text; }

#ifdef ROOTEX_EDITOR
	virtual void draw() override;
#endif
};
