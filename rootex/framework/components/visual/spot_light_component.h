#include "component.h"
#include "common/common.h"

/// Component to apply point lights to the scene, first 4 created instances of this are used
class SpotLightComponent : public Component
{
	static Component* Create(const JSON::json& componentData);
	static Component* CreateDefault();

	friend class EntityFactory;

public:
	static const ComponentID s_ID = (ComponentID)ComponentIDs::SpotLightComponent;

	/// attenuation = 1/(attConst + attLin * r + attQuad * r * r) 
	float m_attConst;
	/// attenuation = 1/(attConst + attLin * r + attQuad * r * r)
	float m_attLin;
	/// attenuation = 1/(attConst + attLin * r + attQuad * r * r)
	float m_attQuad;
	/// Lighting effect clipped for distance > range
	float m_range;
	/// Diffuse intensity of light
	float m_diffuseIntensity;
	/// Diffuse color of light
	Color m_diffuseColor;
	/// Ambient color of light
	Color m_ambientColor;
	/// increasing spot increases the angular attenuation wrt axis
	float m_spot;
	/// Lighting effect clipped for angle > angleRange
	float m_angleRange;
	
	virtual String getName() const override { return "SpotLightComponent"; }
	ComponentID getComponentID() const override { return s_ID; }

	SpotLightComponent::SpotLightComponent(const float constAtt, const float linAtt, const float quadAtt,
	    const float range, const float diffuseIntensity, const Color& diffuseColor, const Color& ambientColor,
		float spot, float angleRange);
	SpotLightComponent(SpotLightComponent&) = delete;
	~SpotLightComponent();
	virtual JSON::json getJSON() const override;

#ifdef ROOTEX_EDITOR
	void draw() override;
#endif // ROOTEX_EDITOR
};
