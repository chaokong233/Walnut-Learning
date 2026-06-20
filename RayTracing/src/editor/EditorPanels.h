#pragma once

#include "EditorContext.h"

#include <string>
#include <unordered_map>

class SceneHierarchyPanel
{
public:
	void OnUIRender(EditorContext& context);
};

class InspectorPanel
{
public:
	void OnUIRender(EditorContext& context);

private:
	std::unordered_map<Entity, std::string> modelPathBuffers_;
};

class RenderSettingsPanel
{
public:
	void OnUIRender(EditorContext& context);
};

class InfoPanel
{
public:
	void OnUIRender(EditorContext& context);
};

class ViewportPanel
{
public:
	void OnUIRender(EditorContext& context);

private:
	void DrawSelectionOverlay(EditorContext& context);
};
