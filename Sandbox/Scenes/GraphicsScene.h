#pragma once
#include <LumosEngine.h>

class GraphicsScene : public Lumos::Scene
{
public:
	GraphicsScene(const std::string& SceneName);
	virtual ~GraphicsScene();

	virtual void OnInit() override;
	virtual void OnCleanupScene() override;
	virtual void OnUpdate(const Lumos::TimeStep& timeStep) override;
	virtual void OnImGui() override;

	void LoadModels();
    
private:
    
    Lumos::Entity m_Terrain;

};
