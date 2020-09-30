#include "render_system.h"

#include "components/visual/fog_component.h"
#include "renderer/shaders/register_locations_vertex_shader.h"
#include "renderer/shaders/register_locations_pixel_shader.h"
#include "light_system.h"
#include "renderer/material_library.h"
#include "components/visual/sky_component.h"
#include "application.h"

RenderSystem* RenderSystem::GetSingleton()
{
	static RenderSystem singleton;
	return &singleton;
}

RenderSystem::RenderSystem()
    : System("RenderSystem", UpdateOrder::Render, true)
	, m_Renderer(new Renderer())
    , m_VSProjectionConstantBuffer(nullptr)
    , m_VSPerFrameConstantBuffer(nullptr)
    , m_PSPerFrameConstantBuffer(nullptr)
    , m_IsEditorRenderPassEnabled(false)
{
	m_Camera = HierarchySystem::GetSingleton()->getRootEntity()->getComponent<CameraComponent>().get();
	m_TransformationStack.push_back(Matrix::Identity);
	setProjectionConstantBuffers();
	
	m_LineMaterial = std::dynamic_pointer_cast<BasicMaterial>(MaterialLibrary::GetMaterial("rootex/assets/materials/line.rmat"));
	m_CurrentFrameLines.m_Endpoints.reserve(LINE_INITIAL_RENDER_CACHE * 2 * 3);
	m_CurrentFrameLines.m_Indices.reserve(LINE_INITIAL_RENDER_CACHE * 2);
}

void RenderSystem::calculateTransforms(HierarchyComponent* hierarchyComponent)
{
	pushMatrix(hierarchyComponent->getOwner()->getComponent<TransformComponent>()->getLocalTransform());
	for (auto&& child : hierarchyComponent->getChildren())
	{
		child->getOwner()->getComponent<TransformComponent>()->m_ParentAbsoluteTransform = getCurrentMatrix();
		calculateTransforms(child);
	}
	popMatrix();
}

void RenderSystem::renderPassRender(RenderPass renderPass)
{
	ModelComponent* mc = nullptr;
	for (auto& component : s_Components[ModelComponent::s_ID])
	{
		mc = (ModelComponent*)component;
		if (mc->getRenderPass() & (unsigned int)renderPass)
		{
			mc->preRender();
			if (mc->isVisible())
			{
				mc->render();
			}
			mc->postRender();
		}
	}

	AnimatedModelComponent* amc = nullptr;
	for (auto& component : s_Components[AnimatedModelComponent::s_ID])
	{
		amc = (AnimatedModelComponent*)component;
		if (amc->getRenderPass() & (unsigned int)renderPass)
		{
			amc->preRender();
			if (amc->isVisible())
			{
				amc->render();
			}
			amc->postRender();
		}
	}
}

void RenderSystem::recoverLostDevice()
{
	ERR("Fatal error: D3D Device lost");
}

void RenderSystem::setConfig(const JSON::json& configData, bool openInEditor)
{
	if (configData.find("camera") != configData.end())
	{
		Ref<Entity> cameraEntity = EntityFactory::GetSingleton()->findEntity(configData["camera"]);
		if (cameraEntity)
		{
			setCamera(cameraEntity->getComponent<CameraComponent>().get());
			return;
		}
	}
	setCamera(EntityFactory::GetSingleton()->findEntity(ROOT_ENTITY_ID)->getComponent<CameraComponent>().get());
}

void RenderSystem::update(float deltaMilliseconds)
{
	Color clearColor = { 0.15f, 0.15f, 0.15f, 1.0f };
	float fogStart = 0.0f;
	float fogEnd = -1000.0f;

	if (!s_Components[FogComponent::s_ID].empty())
	{
		FogComponent* firstFog = (FogComponent*)s_Components[FogComponent::s_ID].front();
		clearColor = firstFog->getColor();

		for (auto& component : s_Components[FogComponent::s_ID])
		{
			FogComponent* fog = (FogComponent*)component;
			clearColor = Color::Lerp(clearColor, fog->getColor(), 0.5f);
			fogStart = fog->getNearDistance();
			fogEnd = fog->getFarDistance();
		}
	}
	Application::GetSingleton()->getWindow()->clearCurrentTarget(clearColor);

	Ref<HierarchyComponent> rootHC = HierarchySystem::GetSingleton()->getRootEntity()->getComponent<HierarchyComponent>();
	calculateTransforms(rootHC.get());

	RenderingDevice::GetSingleton()->setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	RenderingDevice::GetSingleton()->setCurrentRasterizerState();
	RenderingDevice::GetSingleton()->setDepthStencilState();
	RenderingDevice::GetSingleton()->setAlphaBlendState();

	AnimationSystem::GetSingleton()->update(deltaMilliseconds);

	perFrameVSCBBinds(fogStart, fogEnd);
	const Color& fogColor = clearColor;
	perFramePSCBBinds(fogColor);

#ifdef ROOTEX_EDITOR
	if (m_IsEditorRenderPassEnabled)
	{
		renderPassRender(RenderPass::Editor);
		renderLines();
	}
#endif // ROOTEX_EDITOR
	renderPassRender(RenderPass::Basic);
	renderPassRender(RenderPass::Alpha);
	{
		RenderingDevice::GetSingleton()->enableSkyDepthStencilState();
		RenderingDevice::RasterizerState currentRS = RenderingDevice::GetSingleton()->getRasterizerState();
		RenderingDevice::GetSingleton()->setRasterizerState(RenderingDevice::RasterizerState::Sky);
		RenderingDevice::GetSingleton()->setCurrentRasterizerState();
		for (auto& component : s_Components[SkyComponent::s_ID])
		{
			SkyComponent* sky = (SkyComponent*)component;
			for (auto& [material, meshes] : sky->getSkySphere()->getMeshes())
			{
				m_Renderer->bind(sky->getSkyMaterial());
				for (auto& mesh : meshes)
				{
					m_Renderer->draw(mesh.m_VertexBuffer.get(), mesh.m_IndexBuffer.get());
				}
			}
		}
		RenderingDevice::GetSingleton()->setRasterizerState(currentRS);
		RenderingDevice::GetSingleton()->disableSkyDepthStencilState();
	}
}

void RenderSystem::renderLines()
{
	if (m_CurrentFrameLines.m_Endpoints.size())
	{
		m_Renderer->bind(m_LineMaterial.get());

		enableLineRenderMode();

		VertexBuffer vb(m_CurrentFrameLines.m_Endpoints);
		IndexBuffer ib(m_CurrentFrameLines.m_Indices);

		m_Renderer->draw(&vb, &ib);

		m_CurrentFrameLines.m_Endpoints.clear();
		m_CurrentFrameLines.m_Indices.clear();

		resetRenderMode();
	}
}

void RenderSystem::submitLine(const Vector3& from, const Vector3& to)
{
	m_CurrentFrameLines.m_Endpoints.push_back(from.x);
	m_CurrentFrameLines.m_Endpoints.push_back(from.y);
	m_CurrentFrameLines.m_Endpoints.push_back(from.z);

	m_CurrentFrameLines.m_Endpoints.push_back(to.x);
	m_CurrentFrameLines.m_Endpoints.push_back(to.y);
	m_CurrentFrameLines.m_Endpoints.push_back(to.z);

	m_CurrentFrameLines.m_Indices.push_back(m_CurrentFrameLines.m_Indices.size());
	m_CurrentFrameLines.m_Indices.push_back(m_CurrentFrameLines.m_Indices.size());
}

void RenderSystem::pushMatrix(const Matrix& transform)
{
	m_TransformationStack.push_back(transform * m_TransformationStack.back());
}

void RenderSystem::pushMatrixOverride(const Matrix& transform)
{
	m_TransformationStack.push_back(transform);
}

void RenderSystem::popMatrix()
{
	m_TransformationStack.pop_back();
}

void RenderSystem::enableWireframeRasterizer()
{
	RenderingDevice::GetSingleton()->setRasterizerState(RenderingDevice::RasterizerState::Wireframe);
}

void RenderSystem::resetDefaultRasterizer()
{
	RenderingDevice::GetSingleton()->setRasterizerState(RenderingDevice::RasterizerState::Default);
}

void RenderSystem::setProjectionConstantBuffers()
{
	const Matrix& projection = getCamera()->getProjectionMatrix();
	Material::SetVSConstantBuffer(projection.Transpose(), m_VSProjectionConstantBuffer, PER_CAMERA_CHANGE_VS_CPP);
}

void RenderSystem::perFrameVSCBBinds(float fogStart, float fogEnd)
{
	const Matrix& view = getCamera()->getViewMatrix();
	Material::SetVSConstantBuffer(PerFrameVSCB({ view.Transpose(), -fogStart, -fogEnd }), m_VSPerFrameConstantBuffer, PER_FRAME_VS_CPP);
}

void RenderSystem::perFramePSCBBinds(const Color& fogColor)
{
	PerFramePSCB perFrame;
	perFrame.lights = LightSystem::GetSingleton()->getLights();
	perFrame.fogColor = fogColor;
	Material::SetPSConstantBuffer(perFrame, m_PSPerFrameConstantBuffer, PER_FRAME_PS_CPP);
}

void RenderSystem::enableLineRenderMode()
{
	RenderingDevice::GetSingleton()->setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
}

void RenderSystem::resetRenderMode()
{
	RenderingDevice::GetSingleton()->setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void RenderSystem::setCamera(CameraComponent* camera)
{
	m_Camera = camera;
	if (m_Camera)
	{
		setProjectionConstantBuffers();
	}
}

void RenderSystem::restoreCamera()
{
	setCamera(HierarchySystem::GetSingleton()->getRootEntity()->getComponent<CameraComponent>().get());
}

const Matrix& RenderSystem::getCurrentMatrix() const
{
	return m_TransformationStack.back();
}

#ifdef ROOTEX_EDITOR
#include "imgui.h"
void RenderSystem::draw()
{
	System::draw();

	ImGui::Columns(2);

	ImGui::Text("Camera");
	ImGui::NextColumn();
	if (ImGui::BeginCombo("##Camera", RenderSystem::GetSingleton()->getCamera()->getOwner()->getFullName().c_str()))
	{
		for (auto&& camera : System::GetComponents(CameraComponent::s_ID))
		{
			if (ImGui::MenuItem(camera->getOwner()->getFullName().c_str()))
			{
				RenderSystem::GetSingleton()->setCamera((CameraComponent*)camera);
			}
		}

		ImGui::EndCombo();
	}

	ImGui::Columns(1);
}
#endif
