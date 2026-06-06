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
**现状**：场景在 `WalnutApp.cpp` 中硬编码构造，不支持运行时的物体移动或光源改变。
**计划**：
- **Scene 架构**：实现标准的 `Scene` 类，管理 `Entity`（包含 Mesh、Transform、Material）和 `Light`。
- **动态光源**：将光源信息打包放入 SSBO 或 Uniform Buffer 中，允许每帧更新数据而无需重建管线。
- **动态模型**：对网格数据建立静态的 Bottom-Level Acceleration Structure (BLAS)；在每帧根据物体的 Transform 矩阵，动态重建或更新 Top-Level Acceleration Structure (TLAS)。

## 4. 资源管理与配置文件系统
**现状**：资源路径硬编码，缺乏统一管理。
**计划**：
- **配置文件模式**：采用 `总 .ini 文件 -> 分类别 .json 文件` 的架构（参考 Games104 引擎）。
  - `config.ini`：入口配置，记录各个 JSON 的路径。
  - `scene.json`：场景序列化，包含场景中的实体、位置、材质。
  - `assets.json`：全局纹理、模型资源注册表。
  - `shaders.json`：核心 Shader 路径与编译配置。
- **目录重构**：优化当前工程，分离 `src/`、`assets/`、`shaders/`、`config/` 等目录。
- **资源管理器**：实现 `ResourceManager` 类，统一处理模型（tinyobjloader）和纹理（stb_image）的加载、缓存与 Vulkan 资源绑定。

## 5. UI层架构与多线程渲染改造
**现状**：`ExampleLayer` 直接持有 `Renderer`，渲染与 UI 逻辑同步，渲染复杂场景时会导致编辑器卡顿。
**计划**：
- **架构解耦**：将 UI / 逻辑更新与渲染剥离，分离出**主线程**（负责系统事件、UI 绘制、场景逻辑更新）和**渲染线程**（负责 Vulkan 命令录制与提交）。
- **数据同步**：采用双缓冲（Double Buffering）或渲染队列（Render Queue）机制，每帧将主线程的场景快照（Render Packet）同步给渲染线程。

## 6. 编辑器功能完善
**现状**：UI 层残留无效控件，缺乏实际的场景编辑能力。
**计划**：
- **Scene Hierarchy Panel**：展示场景实体树形结构。
- **Inspector Panel**：编辑选中实体的 Transform（平移、旋转、缩放）和材质属性。
- **Render Settings Panel**：
  - 支持切换渲染模式（实时预览低采样率 vs 最终高采样率渲染）。
  - 调整环境光、曝光、降噪（Denoiser）开关等。
- **Viewport Panel**：将渲染结果作为 ImGui Texture 绘制到窗口中，支持类似 Blender 的视口操作。
