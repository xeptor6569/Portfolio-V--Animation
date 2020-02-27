Texture2D tx_diffuse : register(t0);
Texture2D tx_emissive : register(t1);
Texture2D tx_specular : register(t2);
SamplerState samLinear : register(s0);

struct VSOut
{
    float4 pos : SV_POSITION;
    float4 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 world_pos : POSITIONT;
  //  float4 eye_pos : EYEPOS;
};
struct PS_OUTPUT
{
    float4 color : SV_TARGET;
};

cbuffer ConstantLights : register(b0)
{
      // Light
    float4 vLightDir; // 0 Direction(values: direction), 1 Point (values: position), 2 Spot (direction)
    float4 vLightColor;
    float4 vLightPos; // 0 Directional (none), 1 Point (position), 2 Spot (position)
    float4 vRadius;
    float4 specularColor;
    float4 specularPower;
    float3 cameraPos; //eyepos
    float buffer;
    float3 emissive;
    float emissive_factor;
    float3 diffuse;
    float diffuse_factor;
    float3 specular;
    float specular_factor;
    float3 shininess;
    float shininess_factor;
};

static const float4 ambient_light = { 0.25f, 0.25f, 0.25f, 0.0f };
PS_OUTPUT main(VSOut input)
{
    PS_OUTPUT output;
    float4 light_power = { 1.f, 1.f, 1.f, 1.f };
    
    float3 light_dir = vLightPos.xyz - input.world_pos.xyz;
    float sq_distance = dot(light_dir, light_dir);
    
    light_dir = light_dir / sqrt(sq_distance);
    float3 eye_dir = cameraPos.xyz - input.world_pos.xyz;
    float sq_distance_eye = dot(eye_dir, eye_dir);
    float distance_eye = sqrt(sq_distance_eye);
    eye_dir = eye_dir / distance_eye;
    float3 norm = normalize(input.normal.xyz);
    float nl = dot(norm, light_dir);
    float diffuse_intensity = saturate(nl);
    float3 half_vector = normalize(light_dir + eye_dir);
    float nh = dot(norm, half_vector);
    float specular_intensity = pow(saturate(nh), 1 + shininess_factor);
    float4 light_intensity = float4(vLightColor.xyz, 0.0f) * specularPower.y / sq_distance;
    float4 mat_diffuse = tx_diffuse.Sample(samLinear, input.uv) *float4(diffuse, 0.0f) * diffuse_factor;
    float4 mat_specular = tx_specular.Sample(samLinear, input.uv) *float4(specular, 0.0f) * specular_factor;
    float4 mat_emissive = tx_emissive.Sample(samLinear, input.uv) *float4(emissive, 0.0f) * emissive_factor;
    float4 emissive = mat_emissive;
    float4 ambient = mat_diffuse * ambient_light;
    float4 specular = mat_specular * specular_intensity * light_intensity;
    float4 diffuse = mat_diffuse * diffuse_intensity * light_intensity;
    // hacky conservation of energy
    diffuse.xyz -= specular.xyz;
    diffuse.xyz = saturate(diffuse.xyz);
    float4 color = ambient + specular + diffuse + emissive;
        
    output.color = color;
   // output.color = float4(1, 0, 1, 1);
    return output;
}