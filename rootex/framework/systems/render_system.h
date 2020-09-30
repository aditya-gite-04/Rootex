#pragma once

#include "core/renderer/renderer.h"
#include "framework/components/visual/camera_component.h"
#include "framework/system.h"
#include "framework/systems/hierarchy_system.h"
#include "main/window.h"
#include "components/visual/model_component.h"
#include "components/visual/animated_model_component.h"
#include "renderer/render_pass.h"

#define LINE_INITIAL_RENDER_CACHE 1000

class RenderSystem : public System
{
	struct LineRequests
	{
		Vector<float> m_Endpoints;
		Vector<unsigned short> m_Indices;
	};

	CameraComponent* m_Camera;

	Ptr<Renderer> m_Renderer;
	Vector<Matrix> m_TransformationStack;

	Ref<BasicMaterial> m_LineMaterial;
	LineRequests m_CurrentFrameLines;

	Microsoft::WRL::ComPtr<ID3D11Buffer> m_VSPerFrameConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_VSProjectionConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_PSPerFrameConstantBuffer;

	bool m_IsEditorRenderPassEnabled;

	RenderSystem();
	RenderSystem(RenderSystem&) = delete;
	virtual ~RenderSystem() = default;

	void renderPassRender(RenderPass renderPass);

public:
	static RenderSystem* GetSingleton();
	
	void setConfig(const JSON::json& configData, bool openInEditor) override;
	void update(float deltaMilliseconds) override;
	void renderLines();
	void submitLine(const Vector3& from, const Vector3& to);
	void recoverLostDevice();

	void setCamera(CameraComponent* camera);
	void restoreCamera();

	void calculateTransforms(HierarchyComponent* hierarchyComponent);
	void pushMatrix(const Matrix& transform);
	void pushMatrixOverride(const Matrix& transform);
	void popMatrix();
	
	void enableWireframeRasterizer();
	void resetDefaultRasterizer();

	void setProjectionConstantBuffers();
	void perFrameVSCBBinds(float fogStart, float fogEnd);
	void perFramePSCBBinds(const Color& fogColor);

	void setIsEditorRenderPass(bool enabled) { m_IsEditorRenderPassEnabled = enabled; }
	
	void enableLineRenderMode();
	void resetRenderMode();

	CameraComponent* getCamera() const { return m_Camera; }
	const Matrix& getCurrentMatrix() const;
	const Renderer* getRenderer() const { return m_Renderer.get(); }

#ifdef ROOTEX_EDITOR
	void draw() override;
#endif // ROOTEX_EDITOR
};
