#include "EditorPanels.h"

#include "Walnut/Image.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cfloat>
#include <filesystem>
#include <vector>

namespace
{
	void MarkSceneEdited(EditorContext& context)
	{
		if (context.actions && context.actions->MarkSceneEdited)
		{
			context.actions->MarkSceneEdited();
		}
	}

	void MarkCameraEdited(EditorContext& context)
	{
		if (context.actions && context.actions->MarkCameraEdited)
		{
			context.actions->MarkCameraEdited();
		}
	}

	const char* TextureSlotName(MaterialTextureSlot slot)
	{
		switch (slot)
		{
		case MaterialTextureSlot::BaseColor:
			return "Base Color";
		case MaterialTextureSlot::Metallic:
			return "Metallic";
		case MaterialTextureSlot::Roughness:
			return "Roughness";
		case MaterialTextureSlot::Normal:
			return "Normal";
		case MaterialTextureSlot::IBL:
			return "IBL";
		}

		return "Texture";
	}

	bool DrawUIntDrag(const char* label, uint32_t& value, uint32_t minValue = 1, uint32_t maxValue = 4096)
	{
		int editedValue = static_cast<int>(value);
		if (ImGui::DragInt(label, &editedValue, 1.0f, static_cast<int>(minValue), static_cast<int>(maxValue)))
		{
			value = static_cast<uint32_t>(std::max<int>(editedValue, static_cast<int>(minValue)));
			return true;
		}
		return false;
	}

	bool ProjectWorldToViewport(const Camera& camera, const glm::vec3& worldPosition, const EditorState& state, ImVec2& out)
	{
		const glm::vec4 clip = camera.GetProjMatrix() * camera.GetViewMatrix() * glm::vec4(worldPosition, 1.0f);
		if (clip.w <= 0.0001f)
		{
			return false;
		}

		const glm::vec3 ndc = glm::vec3(clip) / clip.w;
		out.x = state.viewportMin.x + (ndc.x * 0.5f + 0.5f) * static_cast<float>(state.viewportWidth);
		out.y = state.viewportMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(state.viewportHeight);
		return true;
	}

	void DrawTextureSlot(EditorContext& context, Entity entity, uint32_t meshIndex, MaterialTextureSlot slot, std::string& path)
	{
		ImGui::PushID(static_cast<int>(slot));
		const std::string label = std::string(TextureSlotName(slot)) + " Texture";
		const bool edited = ImGui::InputText(label.c_str(), &path);
		const bool committed = ImGui::IsItemDeactivatedAfterEdit();
		if (edited)
		{
			context.state->dirty = true;
		}
		if (committed && context.actions && context.actions->ApplyMaterialTexture)
		{
			context.actions->ApplyMaterialTexture(entity, meshIndex, slot, path);
		}

		if (ImGui::SmallButton("Use Drop"))
		{
			context.state->textureDropTarget.entity = entity;
			context.state->textureDropTarget.meshIndex = meshIndex;
			context.state->textureDropTarget.slot = slot;
			context.state->status = std::string("Drop target: ") + TextureSlotName(slot);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Clear"))
		{
			if (context.actions && context.actions->ApplyMaterialTexture)
			{
				context.actions->ApplyMaterialTexture(entity, meshIndex, slot, {});
			}
		}
		if (context.state->textureDropTarget.IsValid() &&
			context.state->textureDropTarget.entity == entity &&
			context.state->textureDropTarget.meshIndex == meshIndex &&
			context.state->textureDropTarget.slot == slot)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("active");
		}
		ImGui::PopID();
	}

	std::vector<std::filesystem::path> EnumerateSceneFiles(const std::filesystem::path& directory)
	{
		std::vector<std::filesystem::path> files;
		std::error_code error;
		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory, error))
		{
			if (error)
			{
				break;
			}
			if (entry.is_regular_file(error) && !error && entry.path().extension() == ".json")
			{
				files.push_back(entry.path());
			}
		}
		std::sort(files.begin(), files.end(), [](const std::filesystem::path& a, const std::filesystem::path& b)
		{
			return a.filename().generic_string() < b.filename().generic_string();
		});
		return files;
	}
}

void SceneHierarchyPanel::OnUIRender(EditorContext& context)
{
	ImGui::Begin("Scene Hierarchy");

	if (ImGui::Button("Square") && context.actions && context.actions->CreatePrimitive)
	{
		context.actions->CreatePrimitive(PrimitiveType::Square);
	}
	ImGui::SameLine();
	if (ImGui::Button("Cube") && context.actions && context.actions->CreatePrimitive)
	{
		context.actions->CreatePrimitive(PrimitiveType::Cube);
	}
	ImGui::SameLine();
	if (ImGui::Button("Sphere") && context.actions && context.actions->CreatePrimitive)
	{
		context.actions->CreatePrimitive(PrimitiveType::Sphere);
	}

	if (ImGui::Button("Radius Light") && context.actions && context.actions->CreateRadiusLight)
	{
		context.actions->CreateRadiusLight();
	}
	ImGui::SameLine();
	if (ImGui::Button("Area Light") && context.actions && context.actions->CreateAreaLight)
	{
		context.actions->CreateAreaLight();
	}

	if (context.state->selectedEntity != InvalidEntity)
	{
		if (ImGui::Button("Delete Selected") && context.actions && context.actions->DeleteSelected)
		{
			context.actions->DeleteSelected();
		}
	}

	if (ImGui::CollapsingHeader("Scenes", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const std::filesystem::path activePath = context.sceneManager->GetActiveScenePath();
		for (const std::filesystem::path& sceneFile : EnumerateSceneFiles(context.sceneManager->GetSceneDirectory()))
		{
			const bool active = !activePath.empty() && std::filesystem::equivalent(sceneFile, activePath);
			if (ImGui::Selectable(sceneFile.filename().generic_string().c_str(), active))
			{
				if (context.actions && context.actions->RequestLoadScene)
				{
					context.actions->RequestLoadScene(sceneFile);
				}
			}
		}
	}

	if (ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::BeginChild("EntitiesList", ImVec2(0.0f, 0.0f), false);
		for (Entity entity : context.scene->GetEntities())
		{
			const TagComponent* tag = context.scene->TryGetTag(entity);
			std::string label = tag ? tag->name : "Entity";
			if (context.scene->TryGetMeshRenderer(entity))
			{
				label += " [Model]";
			}
			else if (context.scene->TryGetRadiusLight(entity))
			{
				label += " [Radius]";
			}
			else if (context.scene->TryGetAreaLight(entity))
			{
				label += " [Area]";
			}
			label += "##" + std::to_string(entity);

			if (ImGui::Selectable(label.c_str(), context.state->selectedEntity == entity))
			{
				context.state->selectedEntity = entity;
			}
		}

		const bool clickedBlankSpace = ImGui::IsWindowHovered() &&
			ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
			!ImGui::IsAnyItemHovered();
		if (clickedBlankSpace)
		{
			context.state->selectedEntity = InvalidEntity;
		}
		ImGui::EndChild();
	}

	ImGui::End();
}

void InspectorPanel::OnUIRender(EditorContext& context)
{
	ImGui::Begin("Inspector");

	const Entity entity = context.state->selectedEntity;
	if (!context.scene->IsValid(entity))
	{
		context.state->selectedEntity = InvalidEntity;
		ImGui::TextDisabled("No entity selected");
		ImGui::End();
		return;
	}

	if (TagComponent* tag = context.scene->TryGetTag(entity))
	{
		if (ImGui::InputText("Name", &tag->name))
		{
			MarkSceneEdited(context);
		}
	}

	if (TransformComponent* transform = context.scene->TryGetTransform(entity))
	{
		bool changed = false;
		changed |= ImGui::DragFloat3("Translation", &transform->translation.x, 0.01f);
		changed |= ImGui::DragFloat3("Rotation", &transform->rotation.x, 0.1f);
		changed |= ImGui::DragFloat3("Scale", &transform->scale.x, 0.01f, 0.001f, 100.0f);
		if (changed)
		{
			MarkSceneEdited(context);
		}
	}

	if (MeshRendererComponent* meshRenderer = context.scene->TryGetMeshRenderer(entity))
	{
		if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Checkbox("Visible", &meshRenderer->visible))
			{
				MarkSceneEdited(context);
			}

			if (meshRenderer->model)
			{
				ImGui::TextWrapped("Source: %s", meshRenderer->model->GetPath().c_str());
				if (!meshRenderer->model->GetLastError().empty())
				{
					ImGui::TextWrapped("Error: %s", meshRenderer->model->GetLastError().c_str());
				}
			}

			std::string& pathBuffer = modelPathBuffers_[entity];
			if (pathBuffer.empty() && meshRenderer->model)
			{
				pathBuffer = meshRenderer->model->GetPath();
			}
			ImGui::InputText("Model Path", &pathBuffer);
			if (ImGui::Button("Rebind Model"))
			{
				std::string error;
				const std::filesystem::path base = context.sceneManager->GetActiveScenePath().parent_path();
				if (context.resourceManager->RebindModel(*context.scene, entity, pathBuffer, &error, base))
				{
					context.state->status = "Model rebound.";
					MarkSceneEdited(context);
				}
				else
				{
					context.state->status = error;
				}
			}
		}

		if (meshRenderer->model && ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
		{
			std::vector<Mesh>& meshes = meshRenderer->model->GetMeshes();
			for (uint32_t meshIndex = 0; meshIndex < meshes.size(); meshIndex++)
			{
				Mesh& mesh = meshes[meshIndex];
				ImGui::PushID(static_cast<int>(meshIndex));
				const std::string meshLabel = mesh.name.empty() ? ("Mesh " + std::to_string(meshIndex)) : mesh.name;
				if (ImGui::TreeNode(meshLabel.c_str()))
				{
					Material& material = mesh.material;
					bool changed = false;
					changed |= ImGui::ColorEdit3("Base Color", &material.BaseColor.x);
					changed |= ImGui::ColorEdit3("Emissive", &material.EmissiveColor.x);
					changed |= ImGui::ColorEdit3("Specular Tint", &material.SpecularTint.x);
					changed |= ImGui::SliderFloat("Roughness", &material.Roughness, 0.0f, 1.0f);
					changed |= ImGui::SliderFloat("Metallic", &material.Metallic, 0.0f, 1.0f);
					changed |= ImGui::SliderFloat("Specular", &material.Specular, 0.0f, 1.0f);
					changed |= ImGui::SliderFloat("Subsurface", &material.Subsurface, 0.0f, 1.0f);
					changed |= ImGui::SliderFloat("Anisotropic", &material.Anisotropic, 0.0f, 1.0f);
					if (changed)
					{
						MarkSceneEdited(context);
					}

					DrawTextureSlot(context, entity, meshIndex, MaterialTextureSlot::BaseColor, material.BaseColorTexturePath);
					DrawTextureSlot(context, entity, meshIndex, MaterialTextureSlot::Metallic, material.MetallicTexturePath);
					DrawTextureSlot(context, entity, meshIndex, MaterialTextureSlot::Roughness, material.RoughnessTexturePath);
					DrawTextureSlot(context, entity, meshIndex, MaterialTextureSlot::Normal, material.NormalTexturePath);
					DrawTextureSlot(context, entity, meshIndex, MaterialTextureSlot::IBL, material.IBLTexturePath);
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
		}
	}

	if (RadiusLightComponent* light = context.scene->TryGetRadiusLight(entity))
	{
		if (ImGui::CollapsingHeader("Radius Light", ImGuiTreeNodeFlags_DefaultOpen))
		{
			bool changed = false;
			changed |= ImGui::ColorEdit3("Color", &light->color.x);
			changed |= ImGui::DragFloat("Intensity", &light->intensity, 0.05f, 0.0f, 1000.0f);
			changed |= ImGui::DragFloat("Radius", &light->radius, 0.01f, 0.001f, 100.0f);
			if (changed)
			{
				MarkSceneEdited(context);
			}
		}
	}

	if (AreaLightComponent* light = context.scene->TryGetAreaLight(entity))
	{
		if (ImGui::CollapsingHeader("Area Light", ImGuiTreeNodeFlags_DefaultOpen))
		{
			bool changed = false;
			changed |= ImGui::ColorEdit3("Color", &light->color.x);
			changed |= ImGui::DragFloat("Intensity", &light->intensity, 0.05f, 0.0f, 1000.0f);
			changed |= ImGui::DragFloat3("Direction", &light->direction.x, 0.01f);
			changed |= ImGui::DragFloat("Width", &light->width, 0.01f, 0.001f, 100.0f);
			changed |= ImGui::DragFloat("Height", &light->height, 0.01f, 0.001f, 100.0f);
			if (changed)
			{
				MarkSceneEdited(context);
			}
		}
	}

	ImGui::End();
}

void RenderSettingsPanel::OnUIRender(EditorContext& context)
{
	ImGui::Begin("Render Settings");

	int mode = context.state->activeRenderMode == RenderMode::Preview ? 0 : 1;
	if (ImGui::Combo("Mode", &mode, "Preview\0Final\0"))
	{
		context.state->activeRenderMode = mode == 0 ? RenderMode::Preview : RenderMode::Final;
	}

	int outputType = static_cast<int>(context.state->outputType);
	if (ImGui::Combo("Output", &outputType, "Final Color\0Albedo\0Normal\0"))
	{
		context.state->outputType = static_cast<RenderOutputType>(outputType);
	}

	if (ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawUIntDrag("Preview Max Samples", context.state->previewSettings.maxSamples, 1, 4096);
		DrawUIntDrag("Preview Bounces", context.state->previewSettings.maxBounceCount, 1, 64);
	}

	if (ImGui::CollapsingHeader("Final Render", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawUIntDrag("Final Max Samples", context.state->finalSettings.maxSamples, 1, 65536);
		DrawUIntDrag("Final Min Samples", context.state->finalSettings.minSamples, 1, 65536);
		DrawUIntDrag("Final Bounces", context.state->finalSettings.maxBounceCount, 1, 64);
		float threshold = static_cast<float>(context.state->finalSettings.noiseThreshold);
		if (ImGui::DragFloat("Noise Threshold", &threshold, 0.0005f, 0.0f, 1.0f, "%.4f"))
		{
			context.state->finalSettings.noiseThreshold = threshold;
		}
		ImGui::Checkbox("Adaptive Noise", &context.state->finalSettings.adaptiveNoise);
		ImGui::Checkbox("Denoise", &context.state->finalSettings.denoise);
	}

	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
	{
		float focusDistance = context.camera->GetFocusDistance();
		if (ImGui::SliderFloat("Focus Distance", &focusDistance, 0.05f, 20.0f, "%.2f"))
		{
			context.camera->SetFocusDistance(focusDistance);
			MarkCameraEdited(context);
		}

		float dofFocusDistance = context.camera->GetDOFFocusDistance();
		if (ImGui::SliderFloat("DOF Focus Distance", &dofFocusDistance, 0.05f, 40.0f, "%.2f"))
		{
			context.camera->SetDOFFocusDistance(dofFocusDistance);
			MarkCameraEdited(context);
		}

		float lensRadius = context.camera->GetDOFLensRadius();
		if (ImGui::SliderFloat("Lens Radius", &lensRadius, 0.0f, 2.0f, "%.3f"))
		{
			context.camera->SetDOFLensRadius(lensRadius);
			MarkCameraEdited(context);
		}

		bool useDOF = context.camera->IsDOFEnabled();
		if (ImGui::Checkbox("Use DOF", &useDOF))
		{
			context.camera->SetUseDOF(useDOF);
			MarkCameraEdited(context);
		}
	}

	ImGui::Checkbox("Selection Overlay", &context.state->overlayEnabled);

	const char* toggleLabel = context.state->activeRenderMode == RenderMode::Preview
		? "Switch to High Samples"
		: "Switch to Preview";
	if (ImGui::Button(toggleLabel) && context.actions && context.actions->ToggleRenderMode)
	{
		context.actions->ToggleRenderMode();
	}

	if (ImGui::Button("Save Scene") && context.actions && context.actions->SaveScene)
	{
		context.actions->SaveScene();
	}

	ImGui::End();
}

void InfoPanel::OnUIRender(EditorContext& context)
{
	ImGui::Begin("Info");
	ImGui::Text("FPS: %.1f", context.state->fps);
	ImGui::Text("Frame Cost: %.3f ms", context.state->frameTimeMs);
	ImGui::Text("Viewport: %u x %u", context.state->viewportWidth, context.state->viewportHeight);
	ImGui::Separator();

	const std::filesystem::path activeScene = context.sceneManager->GetActiveScenePath();
	ImGui::TextWrapped("Scene: %s", activeScene.empty() ? "<none>" : activeScene.generic_string().c_str());
	ImGui::Text("Dirty: %s", context.state->dirty ? "yes" : "no");
	ImGui::Text("Revision: %llu", static_cast<unsigned long long>(context.scene->GetRevision()));

	uint32_t modelCount = 0;
	uint32_t radiusLightCount = 0;
	uint32_t areaLightCount = 0;
	for (Entity entity : context.scene->GetEntities())
	{
		modelCount += context.scene->TryGetMeshRenderer(entity) ? 1 : 0;
		radiusLightCount += context.scene->TryGetRadiusLight(entity) ? 1 : 0;
		areaLightCount += context.scene->TryGetAreaLight(entity) ? 1 : 0;
	}
	ImGui::Text("Entities: %zu", context.scene->GetEntities().size());
	ImGui::Text("Models: %u", modelCount);
	ImGui::Text("Radius Lights: %u", radiusLightCount);
	ImGui::Text("Area Lights: %u", areaLightCount);
	ImGui::Separator();

	const glm::vec3 cameraPosition = context.camera->GetPosition();
	const glm::vec3 cameraFront = context.camera->GetFront();
	ImGui::Text("Camera Position: %.2f %.2f %.2f", cameraPosition.x, cameraPosition.y, cameraPosition.z);
	ImGui::Text("Camera Front: %.2f %.2f %.2f", cameraFront.x, cameraFront.y, cameraFront.z);

	if (!context.state->status.empty())
	{
		ImGui::Separator();
		ImGui::TextWrapped("%s", context.state->status.c_str());
	}

	ImGui::End();
}

void ViewportPanel::OnUIRender(EditorContext& context)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("ViewPort");

	const ImVec2 available = ImGui::GetContentRegionAvail();
	context.state->viewportWidth = static_cast<uint32_t>(std::max(available.x, 0.0f));
	context.state->viewportHeight = static_cast<uint32_t>(std::max(available.y, 0.0f));

	if (context.actions && context.actions->SubmitRenderIfNeeded)
	{
		context.actions->SubmitRenderIfNeeded();
	}

	std::shared_ptr<Walnut::StorageImage> image;
	switch (context.state->outputType)
	{
	case RenderOutputType::FinalColor:
		image = context.renderer->GetFinalImage();
		break;
	case RenderOutputType::Albedo:
		image = context.renderer->GetAlbedoImage();
		break;
	case RenderOutputType::Normal:
		image = context.renderer->GetNormalImage();
		break;
	}

	const ImVec2 imageMin = ImGui::GetCursorScreenPos();
	const ImVec2 imageSize(static_cast<float>(context.state->viewportWidth), static_cast<float>(context.state->viewportHeight));
	if (image && context.state->viewportWidth > 0 && context.state->viewportHeight > 0)
	{
		ImGui::Image(image->GetDescriptorSet(), imageSize);
	}
	else
	{
		ImGui::InvisibleButton("ViewportEmpty", imageSize);
	}

	const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
	context.state->viewportMin = glm::vec2(imageMin.x, imageMin.y);
	context.state->viewportMax = glm::vec2(imageMax.x, imageMax.y);

	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && context.actions && context.actions->PickViewport)
	{
		const ImVec2 mouse = ImGui::GetMousePos();
		context.actions->PickViewport(glm::vec2(mouse.x - imageMin.x, mouse.y - imageMin.y));
	}

	DrawSelectionOverlay(context);

	ImGui::End();
	ImGui::PopStyleVar();
}

void ViewportPanel::DrawSelectionOverlay(EditorContext& context)
{
	if (!context.state->overlayEnabled || !context.scene->IsValid(context.state->selectedEntity) ||
		context.state->viewportWidth == 0 || context.state->viewportHeight == 0)
	{
		return;
	}

	const Entity entity = context.state->selectedEntity;
	const TransformComponent* transform = context.scene->TryGetTransform(entity);
	if (!transform)
	{
		return;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImU32 color = IM_COL32(255, 214, 90, 255);

	if (const MeshRendererComponent* meshRenderer = context.scene->TryGetMeshRenderer(entity))
	{
		if (!meshRenderer->model)
		{
			return;
		}

		bool hasProjectedPoint = false;
		ImVec2 minPoint(FLT_MAX, FLT_MAX);
		ImVec2 maxPoint(-FLT_MAX, -FLT_MAX);
		const glm::mat4 modelMatrix = transform->GetMatrix();
		for (const Mesh& mesh : meshRenderer->model->GetMeshes())
		{
			for (const Vertex& vertex : mesh.vertices)
			{
				ImVec2 projected;
				const glm::vec3 world = glm::vec3(modelMatrix * glm::vec4(vertex.position, 1.0f));
				if (ProjectWorldToViewport(*context.camera, world, *context.state, projected))
				{
					hasProjectedPoint = true;
					minPoint.x = std::min(minPoint.x, projected.x);
					minPoint.y = std::min(minPoint.y, projected.y);
					maxPoint.x = std::max(maxPoint.x, projected.x);
					maxPoint.y = std::max(maxPoint.y, projected.y);
				}
			}
		}

		if (hasProjectedPoint)
		{
			drawList->AddRect(minPoint, maxPoint, color, 0.0f, 0, 2.0f);
		}
		return;
	}

	ImVec2 projected;
	if (!ProjectWorldToViewport(*context.camera, transform->translation, *context.state, projected))
	{
		return;
	}

	if (const RadiusLightComponent* light = context.scene->TryGetRadiusLight(entity))
	{
		drawList->AddCircle(projected, std::max(8.0f, light->radius * 80.0f), color, 32, 2.0f);
		return;
	}

	if (const AreaLightComponent* light = context.scene->TryGetAreaLight(entity))
	{
		drawList->AddCircle(projected, 10.0f, color, 16, 2.0f);
		ImVec2 projectedEnd;
		if (ProjectWorldToViewport(*context.camera, transform->translation + light->direction * 0.3f, *context.state, projectedEnd))
		{
			drawList->AddLine(projected, projectedEnd, color, 2.0f);
		}
	}
}
