#include "ShaderToy.h"
#include <QRegularExpression>
ShaderToy::ShaderToy()
{
	setCode(R"(precision mediump float;
uniform float time; // time
uniform vec2  resolution; // resolution

void main(void){
	vec3 destColor = vec3(0.51, 0.2, 0.1);
	vec2 p = (gl_FragCoord.xy * 2.0 - resolution) / min(resolution.x, resolution.y); 	
	float a = atan(p.y / p.x) * 2.0; // Instead of * 2.0, try * 26 or * 128 and higher
	float l = 0.05 / abs(length(p) - 0.8 + sin(a + time * 4.5) * 0.1);
	destColor *= 1.9+ sin(a + time * 00.13) * 0.03;
	
	vec3 destColor2 = vec3(0.0, 0.2, 0.9);
	vec2 p2 = (gl_FragCoord.xy * 3.0 - resolution) / min(resolution.x, resolution.y); 
	float a2 = atan(p.y / p.x) * 3.0;
	float l2 = 0.05 / abs(length(p) + 0.1 - (tan(time/2.)+0.5) + sin(a + time * 13.5) * (0.1 * l));
	destColor2 *= ( 0.5 + sin(a + time * 00.03) * 0.03 ) * 4.0;
	
	vec3 destColor3 = vec3(0.2, 0.9, 0.35);
	vec2 p3 = (gl_FragCoord.xy * 2.0 - resolution) / min(resolution.x, resolution.y); 
	float a3 = atan(p.y / p.x) * 10.0;
	float l3 = 0.05 / abs(length(p) - 0.4 + sin(a + time * 23.5) * (0.1 * l2));
	destColor3 *= 0.5 + sin(a + time * 10.23) * 0.03;
	
	gl_FragColor = vec4(l*destColor + l2*destColor2 + l3*destColor3, 1.0);
})");
}

void ShaderToy::initResources()
{
	if (code_.isEmpty())
		return;
	vk::Device device = window_->device();
	vk::GraphicsPipelineCreateInfo piplineInfo;
	piplineInfo.stageCount = 2;

	vk::ShaderModule fragShader = window_->createShaderFromCode(EShLangFragment, code_);
	if (!fragShader)
		return;


	vk::ShaderModule vertShader = window_->createShaderFromCode(EShLangVertex, R"(
#version 450
out gl_PerVertex{
	vec4 gl_Position;
};
void main() {
	vec2 outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
}
)");

	vk::PipelineShaderStageCreateInfo piplineShaderStage[2];
	piplineShaderStage[0].stage = vk::ShaderStageFlagBits::eVertex;
	piplineShaderStage[0].module = vertShader;
	piplineShaderStage[0].pName = "main";
	piplineShaderStage[1].stage = vk::ShaderStageFlagBits::eFragment;
	piplineShaderStage[1].module = fragShader;
	piplineShaderStage[1].pName = "main";
	piplineInfo.pStages = piplineShaderStage;

	vk::PipelineVertexInputStateCreateInfo vertexInputState({}, 0, nullptr, 0, nullptr);
	piplineInfo.pVertexInputState = &vertexInputState;

	vk::PipelineInputAssemblyStateCreateInfo vertexAssemblyState({}, vk::PrimitiveTopology::eTriangleList);
	piplineInfo.pInputAssemblyState = &vertexAssemblyState;

	vk::PipelineViewportStateCreateInfo viewportState;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;
	piplineInfo.pViewportState = &viewportState;

	vk::PipelineRasterizationStateCreateInfo rasterizationState;
	rasterizationState.polygonMode = vk::PolygonMode::eFill;
	rasterizationState.cullMode = vk::CullModeFlagBits::eNone;
	rasterizationState.frontFace = vk::FrontFace::eCounterClockwise;
	rasterizationState.lineWidth = 1.0f;
	piplineInfo.pRasterizationState = &rasterizationState;

	vk::PipelineMultisampleStateCreateInfo MSState;
	MSState.rasterizationSamples = window_->sampleCountFlagBits();
	piplineInfo.pMultisampleState = &MSState;

	vk::PipelineDepthStencilStateCreateInfo DSState;
	DSState.depthTestEnable = false;
	DSState.depthWriteEnable = false;
	DSState.depthCompareOp = vk::CompareOp::eLessOrEqual;
	piplineInfo.pDepthStencilState = &DSState;

	vk::PipelineColorBlendStateCreateInfo colorBlendState;
	colorBlendState.attachmentCount = 1;
	vk::PipelineColorBlendAttachmentState colorBlendAttachmentState;
	colorBlendAttachmentState.blendEnable = true;
	colorBlendAttachmentState.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
	colorBlendAttachmentState.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	colorBlendAttachmentState.colorBlendOp = vk::BlendOp::eAdd;
	colorBlendAttachmentState.srcAlphaBlendFactor = vk::BlendFactor::eOne;
	colorBlendAttachmentState.dstAlphaBlendFactor = vk::BlendFactor::eOne;
	colorBlendAttachmentState.alphaBlendOp = vk::BlendOp::eAdd;
	colorBlendAttachmentState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendState.pAttachments = &colorBlendAttachmentState;
	piplineInfo.pColorBlendState = &colorBlendState;

	vk::PipelineDynamicStateCreateInfo dynamicState;
	vk::DynamicState dynamicEnables[] = { vk::DynamicState::eViewport ,vk::DynamicState::eScissor };
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicEnables;
	piplineInfo.pDynamicState = &dynamicState;

	vk::PipelineLayoutCreateInfo piplineLayoutInfo;
	vk::PushConstantRange pcRange;
	pcRange.size = sizeof(ShaderParams);
	pcRange.stageFlags = vk::ShaderStageFlagBits::eFragment;
	piplineLayoutInfo.pushConstantRangeCount = 1;
	piplineLayoutInfo.pPushConstantRanges = &pcRange;
	piplineLayout_ = device.createPipelineLayout(piplineLayoutInfo);
	piplineInfo.layout = piplineLayout_;

	piplineInfo.renderPass = window_->windowRenderPass();

	pipline_ = device.createGraphicsPipeline(window_->pipelineCache(), piplineInfo).value;

	device.destroyShaderModule(vertShader);
	device.destroyShaderModule(fragShader);
}

void ShaderToy::releaseResources()
{
	if (code_.isEmpty())
		return;
	vk::Device device = window_->device();
	device.destroyPipeline(pipline_);
	device.destroyPipelineLayout(piplineLayout_);
}

void ShaderToy::startNextFrame(FrameContext frameCtx)
{
	if (code_.isEmpty()||!pipline_)
		return;
	vk::CommandBuffer cmdBuffer = window_->currentCommandBuffer();
	vk::RenderPassBeginInfo beginInfo;
	beginInfo.renderPass = window_->windowRenderPass();
	beginInfo.framebuffer = frameCtx.frameBuffer;
	beginInfo.renderArea.extent.width = frameCtx.viewport.width();
	beginInfo.renderArea.extent.height = frameCtx.viewport.height();
	cmdBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipline_);
	ShaderParams params;
	params.mouse = QVector2D ((QCursor::pos().x() / (float)window_->width()), (window_->height() - QCursor::pos().y()) / (float)window_->height());
	params.resolution = QVector2D(window_->swapChainImageSize().width(), window_->swapChainImageSize().height());
	params.time = frameCtx.time;
	cmdBuffer.pushConstants<ShaderParams>(piplineLayout_, vk::ShaderStageFlagBits::eFragment, 0, params);
	cmdBuffer.draw(4, 1, 0, 0);
	cmdBuffer.endRenderPass();
}

void ShaderToy::setCode(QString code){
	code.remove(QRegularExpression("\\#version +\\d+"));
	code.remove(QRegularExpression("uniform +float +time;"));
	code.remove(QRegularExpression("uniform +vec2 +mouse;"));
	code.remove(QRegularExpression("uniform +vec2 +resolution;"));
	code.prepend(R"(#version 330
layout(push_constant) uniform PushConstant{
 	vec2 mouse;
	vec2 resolution;
	float time;
}pushConstant;
layout(location = 0) out vec4 fragColor;

#define gl_FragColor fragColor
#define time pushConstant.time
#define mouse pushConstant.mouse
#define resolution pushConstant.resolution
)");
	code_ = code.toLocal8Bit();
	qDebug() << code;
	resetVkSource();
}

