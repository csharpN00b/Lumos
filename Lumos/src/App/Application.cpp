#include "LM.h"
#include "Application.h"

#include "Scene.h"
#include "SceneManager.h"
#include "Editor.h"
#include "Input.h"
#include "Engine.h"
#include "Window.h"

#include "Graphics/API/Renderer.h"
#include "Graphics/API/GraphicsContext.h"
#include "Graphics/API/Textures/Texture2D.h"
#include "Graphics/RenderManager.h"
#include "Graphics/Layers/LayerStack.h"
#include "Graphics/Layers/ImGuiLayer.h"
#include "Graphics/Camera/Camera.h"

#include "Entity/EntityManager.h"
#include "Entity/ComponentManager.h"

#include "Utilities/CommonUtils.h"
#include "Utilities/TimeStep.h"
#include "Utilities/AssetsManager.h"
#include "System/VFS.h"
#include "System/JobSystem.h"

#include "Events/ApplicationEvent.h"
#include "Audio/AudioManager.h"
#include "Physics/B2PhysicsEngine/B2PhysicsEngine.h"
#include "Physics/LumosPhysicsEngine/LumosPhysicsEngine.h"

#include <imgui/imgui.h>

namespace Lumos
{
	Application* Application::s_Instance = nullptr;

	Application::Application(const WindowProperties& properties)
		: m_UpdateTimer(0), m_Frames(0), m_Updates(0)
	{
		LUMOS_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

#ifdef  LUMOS_EDITOR
		m_Editor = new Editor(this, properties.Width, properties.Height);
#endif
		Graphics::GraphicsContext::SetRenderAPI(static_cast<Graphics::RenderAPI>(properties.RenderAPI));

		Engine::Instance();

		m_TimeStep = std::make_unique<TimeStep>(0.0f);
		m_Timer = std::make_unique<Timer>();

		const String root = ROOT_DIR;

		VFS::Get()->Mount("CoreShaders", root + "/lumos/res/shaders");
		VFS::Get()->Mount("CoreMeshes", root + "/lumos/res/meshes");
		VFS::Get()->Mount("CoreTextures", root + "/lumos/res/textures");
		VFS::Get()->Mount("CoreFonts", root + "/lumos/res/fonts");

		m_Window = std::unique_ptr<Window>(Window::Create(properties));
		m_Window->SetEventCallback(BIND_EVENT_FN(Application::OnEvent));

		m_SceneManager = std::make_unique<SceneManager>();

		ImGui::CreateContext();
		ImGui::StyleColorsDark();
	}

	Application::~Application()
	{
#ifdef  LUMOS_EDITOR
		delete m_Editor;
#endif
		ImGui::DestroyContext();
	}

	void Application::ClearLayers()
	{
		m_LayerStack->Clear();
	}

	void Application::Init()
	{
		//Initialise the Window
		if (!m_Window->HasInitialised())
			Quit(true, "Window failed to initialise!");

		uint screenWidth = m_Window->GetWidth();
		uint screenHeight = m_Window->GetHeight();

		Lumos::Input::Create();

		Graphics::GraphicsContext::GetContext()->Init();

#ifdef  LUMOS_EDITOR
		m_Editor->OnInit();
#endif

		Graphics::Renderer::Init(screenWidth, screenHeight);

		System::JobSystem::Execute([] { LumosPhysicsEngine::Instance(); LUMOS_CORE_INFO("Initialised LumosPhysics"); });
		System::JobSystem::Execute([] { B2PhysicsEngine::Instance(); LUMOS_CORE_INFO("Initialised B2Physics"); });

        m_AudioManager = std::unique_ptr<AudioManager>(AudioManager::Create());
        m_AudioManager->OnInit();
        
        //Graphics Loading on main thread
        AssetsManager::InitializeMeshes();
        m_RenderManager = std::make_unique<Graphics::RenderManager>(screenWidth, screenHeight);

        System::JobSystem::Wait();
        
        m_LayerStack = new LayerStack();
        PushOverLay(new ImGuiLayer(false));
        
		m_Systems.emplace_back(m_AudioManager.get());
		m_Systems.emplace_back(LumosPhysicsEngine::Instance());
		m_Systems.emplace_back(B2PhysicsEngine::Instance());
        
		m_CurrentState = AppState::Running;
	}

	int Application::Quit(bool pause, const std::string &reason)
	{
		Engine::Release();
		LumosPhysicsEngine::Release();
		B2PhysicsEngine::Release();
		Input::Release();
		AssetsManager::ReleaseMeshes();

		delete m_LayerStack;

		m_SceneManager.release();

		Graphics::Renderer::Release();

		if (pause)
		{
			LUMOS_CORE_ERROR("{0}", reason);
		}

		return 0;
	}

	void Application::SetActiveCamera(Camera* camera) 
	{ 
		m_ActiveCamera = camera; 
		m_AudioManager->SetListener(camera);
	}

	Maths::Vector2 Application::GetWindowSize() const
	{
		return Maths::Vector2(static_cast<float>(m_Window->GetWidth()), static_cast<float>(m_Window->GetHeight()));
	}

	bool Application::OnFrame()
	{
		float now = m_Timer->GetMS(1.0f) * 1000.0f;

#ifdef LUMOS_LIMIT_FRAMERATE
		if (now - m_UpdateTimer > Engine::Instance()->TargetFrameRate())
		{
			m_UpdateTimer += Engine::Instance()->TargetFrameRate();
#endif
            
			m_TimeStep->Update(now);

			{
				OnUpdate(m_TimeStep.get());
				m_Updates++;
			}

			{
				OnRender();
				m_Frames++;
			}

			Input::GetInput().ResetPressed();
			m_Window->OnUpdate();
            
			if (Input::GetInput().GetKeyPressed(LUMOS_KEY_ESCAPE))
				m_CurrentState = AppState::Closing;
#ifdef LUMOS_LIMIT_FRAMERATE
		}
#endif

		if (m_Timer->GetMS() - m_SecondTimer > 1.0f)
		{
			m_SecondTimer += 1.0f;
			Engine::Instance()->SetFPS(m_Frames);
			Engine::Instance()->SetUPS(m_Updates);
			Engine::Instance()->SetFrametime(1000.0f / m_Frames);

			m_Frames = 0;
			m_Updates = 0;
			m_SceneManager->GetCurrentScene()->OnTick();
		}

		if (m_EditorState == EditorState::Next)
			m_EditorState = EditorState::Paused;


		m_SceneManager->ApplySceneSwitch();

		return m_CurrentState != AppState::Closing;
	}

	void Application::OnRender()
	{
		if (m_LayerStack->GetCount() > 0)
		{
			Graphics::Renderer::GetRenderer()->Begin();

			m_LayerStack->OnRender(m_SceneManager->GetCurrentScene());

			Graphics::Renderer::GetRenderer()->Present();
		}
	}

	void Application::OnUpdate(TimeStep* dt)
	{
		const uint sceneIdx = m_SceneManager->GetCurrentSceneIndex();
		const uint sceneMax = m_SceneManager->SceneCount();

        if (Input::GetInput().GetKeyPressed(InputCode::Key::P)) LumosPhysicsEngine::Instance()->SetPaused(!LumosPhysicsEngine::Instance()->IsPaused());
		if (Input::GetInput().GetKeyPressed(InputCode::Key::P)) B2PhysicsEngine::Instance()->SetPaused(!B2PhysicsEngine::Instance()->IsPaused());

		if (Input::GetInput().GetKeyPressed(InputCode::Key::J)) CommonUtils::AddSphere(m_SceneManager->GetCurrentScene());
		if (Input::GetInput().GetKeyPressed(InputCode::Key::K)) CommonUtils::AddPyramid(m_SceneManager->GetCurrentScene());
		if (Input::GetInput().GetKeyPressed(InputCode::Key::L)) CommonUtils::AddLightCube(m_SceneManager->GetCurrentScene());

		if (Input::GetInput().GetKeyPressed(InputCode::Key::E)) m_SceneManager->SwitchScene((sceneIdx + 1) % sceneMax);
		if (Input::GetInput().GetKeyPressed(InputCode::Key::Q)) m_SceneManager->SwitchScene((sceneIdx == 0 ? sceneMax : sceneIdx) - 1);
		if (Input::GetInput().GetKeyPressed(InputCode::Key::R)) m_SceneManager->SwitchScene(sceneIdx);
		if (Input::GetInput().GetKeyPressed(InputCode::Key::V)) m_Window->ToggleVSync();

#ifdef LUMOS_EDITOR
		if (Application::Instance()->GetEditorState() != EditorState::Paused && Application::Instance()->GetEditorState() != EditorState::Preview)
#endif
		{
			m_SceneManager->GetCurrentScene()->OnUpdate(m_TimeStep.get());
			
			for (auto& System : m_Systems)
            {
                System->OnUpdate(m_TimeStep.get());
            }
		}

		m_LayerStack->OnUpdate(m_TimeStep.get(), m_SceneManager->GetCurrentScene());
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Application::OnWindowResize));

		m_LayerStack->OnEvent(e);

		if (e.Handled())
			return;

		m_SceneManager->GetCurrentScene()->OnEvent(e);

		Input::GetInput().OnEvent(e);
	}

	void Application::Run()
	{
		m_UpdateTimer = m_Timer->GetMS(1.0f);
		while (OnFrame())
		{

		}

		Quit();
	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack->PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverLay(Layer* overlay)
	{
		m_LayerStack->PushOverlay(overlay);
		overlay->OnAttach();
	}

	void Application::OnNewScene(Scene * scene)
	{
#ifdef LUMOS_EDITOR
		m_Editor->OnNewScene(scene);
#endif
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_CurrentState = AppState::Closing;
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent &e)
	{
        auto windowSize = GetWindowSize();

		uint width = 1;
		uint height = 1;

		if (e.GetWidth() != 0) width = e.GetWidth();
		if (e.GetHeight() != 0) height = e.GetHeight();

		m_RenderManager->OnResize(width, height);
		Graphics::Renderer::GetRenderer()->OnResize(width, height);
		return false;
	}

	void Application::OnImGui()
	{
#ifdef LUMOS_EDITOR
		if(m_AppType == AppType::Editor)
			m_Editor->OnImGui();
#endif
    }
}
