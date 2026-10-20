//-------------- {pc}
//-------------- {vertex}

#version 330

layout(location = 0) in vec2 a_TexCoord;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Position;
layout(location = 3) in vec4 a_Color;

out vec2 v_TexCoord;
out vec3 v_Normal;
out vec3 v_FragPos;
out vec4 v_Color;

uniform mat4 model; //Model matrice position, rotation and scale
uniform mat4 MVP; // Model View Projection
uniform mat3 normalMatrix;

void main()
{
	gl_Position = MVP * vec4(a_Position, 1);
	v_FragPos = vec3(model * vec4(a_Position, 1));
	v_TexCoord = a_TexCoord;
	v_Normal = normalMatrix * a_Normal; //TODO Check an object with a bigger scale and with a 	offsetPosition, fix : add to offset * rotation this : * offsetPosition * scale
	v_Color = a_Color;
}

//-------------- {fragment}

#version 330

in vec3 v_Normal;
in vec3 v_FragPos;
in vec4 v_Color;
in vec2 v_TexCoord;

uniform vec4 color;
uniform vec2 tiling;
uniform vec2 offset;
uniform vec3 ambientLight;

struct DirectionalLight 
{
	vec3 direction;
	vec3 color;
};

struct PointLight 
{
	vec3 position;
	vec3 color;
	vec3 light_data; // x = constant, y = linear, z = quadratic
};

struct SpotLight 
{
	vec3 position;
	vec3 direction;

	vec3 color;
	vec4 light_data; // x = linear, y = quadratic, z = cutOff, w = outerCutOff
};

uniform sampler2D diffuse;

#define NR_POINT_LIGHTS 10 
uniform PointLight pointLights[NR_POINT_LIGHTS];
#define NR_SPOT_LIGHTS 10
uniform SpotLight spotLights[NR_SPOT_LIGHTS];
#define NR_DIRECTIONAL_LIGHTS 10
uniform DirectionalLight directionalLights[NR_DIRECTIONAL_LIGHTS];

layout(std140) uniform LightIndices
{
	int usedPointLightCount;
	int usedSpotLightCount;
	int usedDirectionalLightCount;
	int padding0;

	ivec4 pointLightsIndices[NR_POINT_LIGHTS];
	ivec4 spotLightsIndices[NR_SPOT_LIGHTS];
	ivec4 directionalLightsIndices[NR_DIRECTIONAL_LIGHTS];
} lightIndices;

vec3 CalculateDirectionalLight(DirectionalLight light2, vec3 norm, vec3 fragPos) 
{
	vec3 lightDir = normalize(-light2.direction); // Directional light
	float diff = max(dot(norm, lightDir), 0.0); //If the light is behind the face, diff is 0
	vec3 diffuse = (diff * vec3(texture(diffuse, (v_TexCoord * tiling) + offset))) * light2.color * 2; //Set the light color and intensity

	return diffuse;
}

vec3 CalculatePointLight(PointLight light2, vec3 norm, vec3 fragPos) 
{
	vec3 lightVec = light2.position - fragPos;
	vec3 lightDir = normalize(light2.position - fragPos); //Direction of the point light between the light source and the face
	float diff = max(dot(norm, lightDir), 0.0); //If the light is behind the face, diff is 0
	vec3 diffuse = (diff * vec3(texture(diffuse, (v_TexCoord * tiling) + offset))) * light2.color * 2; //Set the light color and intensity

	float distanceSq = dot(lightVec, lightVec); // distance square
	float attenuation = 1.0f / (light2.light_data.x + light2.light_data.y * sqrt(distanceSq) + light2.light_data.z * distanceSq);

	vec3 result = (diffuse * attenuation);
	return result;
}

vec3 CalculateSpotLight(SpotLight light2, vec3 norm, vec3 fragPos) 
{
	vec3 lightDir = normalize(light2.position - fragPos); //Direction of the point light between the light source and the face
	float diff = max(dot(norm, lightDir), 0.0); //If the light is behind the face, diff is 0
	vec3 diffuse = (diff * vec3(texture(diffuse, (v_TexCoord * tiling) + offset))) * light2.color * 2; //Set the light color and intensity

	float distance = length(light2.position - fragPos);
	float attenuation = 1.0 / (1 + light2.light_data.x * distance + light2.light_data.y * (distance * distance));

	float theta = dot(lightDir, normalize(-light2.direction));
	float epsilon = light2.light_data.z - light2.light_data.w;
	float intensity = clamp((theta - light2.light_data.w) / epsilon, 0.0, 1.0);
	diffuse *= intensity;

	//Result
	vec3 result = (diffuse * attenuation); //Set face result
	return result;
}

void main()
{
	vec4 textureFrag = texture(diffuse, (v_TexCoord * tiling) + offset);

	vec3 norm = normalize(v_Normal); //Direction of normals

	//Result
	vec3 result = vec3(0,0,0); //Set face result
	// for (int i = 0; i < lightIndices.usedPointLightCount; i++)
	// {
	// 	result += CalculatePointLight(pointLights[lightIndices.pointLightsIndices[i].x], norm, v_FragPos);
	// }	
	// for (int i = 0; i < lightIndices.usedSpotLightCount; i++)
	// {
	// 	result += CalculateSpotLight(spotLights[lightIndices.spotLightsIndices[i].x], norm, v_FragPos);
	// }
	for (int i = 0; i < lightIndices.usedDirectionalLightCount; i++)
	{
		result += CalculateDirectionalLight(directionalLights[lightIndices.directionalLightsIndices[i].x], norm, v_FragPos);
	}
	result += textureFrag.xyz * ambientLight;

	float alpha = textureFrag.w * color.w;

	gl_FragColor = vec4(result * color.xyz, alpha) * v_Color; //Add texture color
}

//-------------- {ps3}
//-------------- {vertex}

void main
(
	float3 a_Position : POSITION,
	float3 a_Normal : NORMAL,
	float2 a_Texcoord : TEXCOORD0,
	float4 a_Color : COLOR0,

	uniform float4x4 model,
	uniform float4x4 MVP,
	uniform float3x3 normalMatrix,

	out float4 ePosition : POSITION,
	out float3 v_Normal : TEXCOORD0,
	out float2 v_Texcoord : TEXCOORD1,
	out float3 v_FragPos : TEXCOORD2,
	out float4 v_Color : COLOR0
)
{
	ePosition = mul(float4(a_Position, 1.0f), MVP);
	v_FragPos = float3(mul(float4(a_Position, 1.0f), model));	
	v_Normal = mul(a_Normal, normalMatrix);
	v_Texcoord = a_Texcoord;
	v_Color = a_Color;
}

//-------------- {fragment}

struct DirectionalLight 
{
	float3 direction;
	float3 color;
};

struct PointLight 
{
	float3 position;

	float3 color;

	float3 light_data; // x = constant, y = linear, z = quadratic
};

struct SpotLight 
{
	float3 position;
	float3 direction;

	float3 color;
	float4 light_data; // x = linear, y = quadratic, z = cutOff, w = outerCutOff
	// float constant;
	// float linear;
	// float quadratic;
	// float cutOff;
	// float outerCutOff;
};

#define NR_POINT_LIGHTS 10
#define NR_SPOT_LIGHTS 10
#define NR_DIRECTIONAL_LIGHTS 10

float3 CalculateDirectionalLight(DirectionalLight light, float3 norm, float4 textureFrag) 
{
	//float3 lightDir = normalize(-light.direction); // Directional light
	float diff = max(dot(norm, -light.direction), 0.0f); //If the light is behind the face, diff is 0
	float3 tx = (diff * textureFrag.xyz) * light.color; // Set the light color and intensity
	return tx;
}

float3 CalculateSpotLight(SpotLight light, float3 norm, float3 fragPos, float4 textureFrag) 
{
	float3 lightVec = light.position - fragPos;
	float3 lightDir = normalize(lightVec); //Direction of the light between the light source and the face
	float diff = max(dot(norm, lightDir), 0.0f); //If the light is behind the face, diff is 0
	float3 diffuse = (diff * textureFrag.xyz) * light.color * 2;

	//vec3 lightDir = normalize(light.position - fragPos); //Direction of the point light between the light source and the face
	//float diff = max(dot(norm, lightDir), 0.0); //If the light is behind the face, diff is 0
	//vec3 diffuse = (diff * textureColor) * light.color * 2; //Set the light color and intensity

	//float distance = length(light.position - fragPos);
	//float attenuation = 1.0 / (light.constant + light.light_data * distance + light.quadratic * (distance * distance));

	float distanceSq = dot(lightVec, lightVec); // distance�
	float attenuation = 1.0 / (1 + light.light_data.x * sqrt(distanceSq) + light.light_data.y * distanceSq);

	float theta = dot(lightDir, normalize(-light.direction));
	float epsilon = light.light_data.z - light.light_data.w;
	float intensity = clamp((theta - light.light_data.w) / epsilon, 0.0f, 1.0f);
	diffuse *= intensity;

	return diffuse * attenuation;
}

float3 CalculatePointLight(PointLight light, float3 norm, float3 fragPos, float4 textureFrag) 
{
	float3 lightVec = light.position - fragPos;
	float3 lightDir = normalize(lightVec); //Direction of the light between the light source and the face
	float diff = max(dot(norm, lightDir), 0.0f); //If the light is behind the face, diff is 0
	float3 diffuse = (diff * textureFrag.xyz) * light.color; //Set the light color and intensity

	float distanceSq = dot(lightVec, lightVec); // distance�
	float attenuation = 1.0f / (1 + light.light_data.y * sqrt(distanceSq) + light.light_data.z * distanceSq);

	return diffuse * attenuation;
}

void main
(
	float3 v_Normal : TEXCOORD0,
	float2 v_Texcoord : TEXCOORD1,
	float3 v_FragPos : TEXCOORD2,
	float4 v_Color : COLOR0,

	uniform sampler2D texture,
	// uniform sampler2D lightingDataTexture,

	uniform float3 ambientLight,
	uniform vec4 color,
	uniform vec2 tiling,
	uniform vec2 offset,
	
	uniform PointLight pointLights[NR_POINT_LIGHTS],
	uniform SpotLight spotLights[NR_SPOT_LIGHTS],
	uniform DirectionalLight directionalLights[NR_DIRECTIONAL_LIGHTS],

	out float4 oColor
)
{
	float4 textureFrag = tex2D(texture, (v_Texcoord * tiling) + offset);

	float3 result = float3(0,0,0);
	result += textureFrag.xyz * ambientLight;
	//result += tex2D(lightingDataTexture, float2(1, 1)).xyz;
	float alpha = textureFrag.w * color.w;
	float3 N = normalize(v_Normal);

	result += CalculateDirectionalLight(directionalLights[1], N, textureFrag);

	//result += CalculateSpotLight(spotLights[1], N, v_FragPos, textureFrag);

	//result += CalculatePointLight(pointLights[1], N, v_FragPos, textureFrag);
	//result += CalculatePointLight(pointLights[2], N, v_FragPos, textureFrag);

	result *= color.xyz;

	oColor = float4(result, alpha) * v_Color;
}
