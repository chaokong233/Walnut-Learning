# Vulkan Path Tracer - Development Plan

基于目前的现状（从 CPU 渲染器 hacking 而来、UI 耦合严重、资产硬编码、使用了 raw Vulkan RT API 导致缺乏动态更新能力），我们制定了以下通用路径追踪渲染器的开发计划：

## 1. 封装 Vulkan Raytracing API
**现状**：`Renderer.cpp` 中充斥着大量的原始 Vulkan RT API（如 `createBottomLevelAccelerationStructure` 等）。
**计划**：
- 提取并封装底层 Vulkan API 到独立的类中（例如 `VulkanRTHelper` 或 `VulkanBackend`）。
- 建立更高级的抽象，如 `AccelerationStructure`、`RTPipeline`、`ShaderBindingTable`，以简化主渲染循环，并为后续动态重构加速结构做好准备。

## 2. 清理 CPU 渲染残余代码
**现状**：保留了大量基于 CPU 的光线追踪代码（如 `#ifndef VULKAN_RT` 块、`MT_Vertical_Iter` 线程池渲染等），增加了维护成本。
**计划**：
- 彻底移除 `Renderer.cpp` / `Renderer.h` 中所有的 CPU 渲染路径逻辑。
- 移除多余的 CPU 层面的材质、相交检测的软实现代码，使得引擎完全专注且依赖于 Vulkan Raytracing 硬件管线。

## 3. 动态场景渲染支持
**现状**：场景在 `Renderer.cpp` 中硬编码，并在`WalnutApp.cpp`直接显示指定，renderer渲染的是硬编码的模型和光源，而不是动态场景。不支持运行时的物体移动或光源改变。
**计划**：
- **Scene 架构**：实现标准的 `Scene` 类，管理 `Entity`（包含 Mesh、Transform、Texture、Material）和 `Light`。现在支持两种光源 `AreaLight` 和 `RadiusLight`。
- **动态光源**：将光源信息打包放入 SSBO 或 Uniform Buffer 中，允许每帧更新数据而无需重建管线。
- **动态模型**：对网格数据建立静态的 Bottom-Level Acceleration Structure (BLAS)；在每帧根据物体的 Transform 矩阵，动态重建或更新 Top-Level Acceleration Structure (TLAS)。支持动态的Material属性，Material属性通过geometryNodeBuffer_上传，更新这个buffer即可。

 
## 4. 资源管理与配置文件系统
**现状**：资源路径硬编码，缺乏统一管理。
**计划**：
- **配置文件模式**：采用 `总 .ini 文件 -> 分类别 .json 文件` 的架构（参考 Games104 引擎）。
  - `config.ini`：入口配置，记录各个 JSON 的路径。
  - `scene.json`：场景序列化，包含场景中的实体、位置、材质。
  - `assets.json`：全局纹理、模型资源注册表。
  - `shaders.json`：核心 Shader 路径与编译配置。
- **打包目录**：在premake文件里添加编译后处理的命令语句，将核心的shaders，configs文件和自带的用于测试和作为默认项的模型和贴图，自动打包至可执行文件的目录下。
- **资源管理器**：实现 `ResourceManager` 类，统一处理模型和纹理（stb_image）的加载、缓存与 Vulkan 资源绑定。

## UI层和编辑器需求
### UI层设施：
  **现状**：raytracing的结果直接输出到视口图像上。
  **需求**：
  - **overlay层**：添加新的图层以绘制编辑器提示，如gizmo，选中的描边等。可能需要额外的光栅管线。overlay层应该是可开关的。
  - **更完善的ui架构**：目前的绘制完全在walnutApp.cpp里进行，未来的复杂编辑器功能会使这里变得冗杂，所以应当重构这一处。
  
### 编辑器功能：
  要具备基本的编辑器功能，可以编辑模型，光源的位置旋转缩放等等。目前可以不具备模型顶点编辑（即通常意义下的建模）的功能，但是要能调整模型的材质（参数，贴图）。
  **模型编辑** 允许创建默认模型（square，cube，sqhere，monkey）。允许从外部资源管理器拖动导入模型（目前只支持.fbx格式），并调整位置旋转缩放。
允许删除模型。允许选中模型 调整材质（参数，贴图）.
  **贴图设置** 选中模型后，可以拖动外部资源管理器的贴图（.png,.jpg）到材质面板的对应槽位（basecolor，metallic等）上，设置材质上的贴图。、
  **资源加载卸载** 在传统的编辑器内，cpu加载的渲染资源 不等于 上传到gpu的渲染资源。为了简化这一步处理，先不要内置的资源浏览器，我们的资源完全来自外部资源管理器的拖入。当拖入模型或贴图时（或更换场景），加载并上传资源，当删除资源，使场景内没有对应资源时，卸载资源。即cpu加载和上传gpu的时机完全等同，程序只做渲染，不保留额外资源。
  **场景加载保存** ctrl s保存场景。点开场景按钮，显示scene路径下读取到的场景，可以选择加载。注意退出和切换场景时 如果场景未保存，显示是否保存的提醒。
  **基本信息** 我希望保留目前ui上的基础信息（帧率，渲染耗时，视口分辨率等等），摄像机设置信息。再加上现在的场景信息。
  **渲染设置调整** 目前调整采样光线数全部依靠改动sample count，我希望像blender一样，可以单独调整两种模式的max（min）sample count，noise threshold等，并有一个切换按钮，切换两种模式。
  **点选模型和光源** 目前项目没有物理库，可以导入一个以实现简单的光线检测（或使用已有的bvh库简单实现一个），以完成点选的操作。暂时不考虑多选资源的情况，即不考虑框选操作。
  **具体面板设计** 一个面板显示基本信息，渲染设置信息。一个面板显示选中entity信息，选中模型和光源时，视口上显示外描边的overlay提示，entity面板上显示模型信息，材质设置，光源属性设置等。
  **gizmo** 暂时先不考虑拖动箭头修改位置等编辑器功能。

## 5. UI层架构与多线程渲染改造
**现状**：`ExampleLayer` 直接持有 `Renderer`，渲染与 UI 逻辑同步，渲染复杂场景时会导致编辑器卡顿。
**计划**：
- **架构解耦**：将 UI / 逻辑更新与渲染剥离，分离出**主线程**（负责系统事件、UI 绘制、场景逻辑更新）和**渲染线程**（负责 Vulkan 命令录制与提交）。
- **overlay层**: 编辑器中用于编辑提示的图层，如选中物体时的描边，和gizmo等提示等，可开关。
- **数据同步**：采用渲染队列（Render Queue）机制，每帧将主线程的场景快照（Render Packet）同步给渲染线程。

## 6. 编辑器功能完善
**现状**：UI 层残留无效控件，缺乏实际的场景编辑能力。
**计划**：
- **Scene Hierarchy Panel**：展示场景实体树形结构。
- **Inspector Panel**：编辑选中实体的 Transform（平移、旋转、缩放）和材质属性。
- **Render Settings Panel**：
  - 支持切换渲染模式（实时预览低采样率 vs 最终高采样率渲染）。
  - 调整最大\小sample数，自适应采样阈值，降噪（Denoiser）开关等。
- **Viewport Panel**：将渲染结果作为 ImGui Texture 绘制到窗口中，支持视口操作。