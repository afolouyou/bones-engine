#version 450

layout(location = 0) in vec2 v_TexCoord;

layout(location = 0) out vec4 o_Color;

layout(std140, binding = 0) uniform Params
{
	vec4 tint;
};

layout(binding = 1) uniform sampler2D u_Texture;

void main()
{
	o_Color = texture(u_Texture, v_TexCoord) * tint;
}
