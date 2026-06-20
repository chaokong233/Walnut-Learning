#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "editor/EditorLayer.h"

#include <memory>

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Ray Tracing";

	Walnut::Application* app = new Walnut::Application(spec);
	auto editorLayer = std::make_shared<EditorLayer>();
	app->PushLayer(editorLayer);
	app->SetMenubarCallback([app, editorLayer]()
	{
		editorLayer->RenderMainMenu(*app);
	});
	return app;
}
