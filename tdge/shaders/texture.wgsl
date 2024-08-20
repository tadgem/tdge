struct Uniforms {
  modelViewProjectionMatrix : mat4x4f,
}

struct VertexInput
{
    @location(0) position : vec4f,
    @location(1) uv : vec2f
}

struct VertexOutput {
  @builtin(position) Position : vec4f,
  @location(0) fragUV : vec2f,
  @location(1) fragPosition: vec4f,
}

@group(0) @binding(0) var<uniform> uniforms : Uniforms;

@group(1) @binding(0) var mySampler: sampler;
@group(1) @binding(1) var myTexture: texture_2d<f32>;

@vertex
fn main(input : VertexInput) -> VertexOutput {
  var output : VertexOutput;
  output.Position = uniforms.modelViewProjectionMatrix * position;
  output.fragUV = uv;
  output.fragPosition = 0.5 * (position + vec4(1.0, 1.0, 1.0, 1.0));
  return output;
}

@fragment
fn main(
@location(0) fragUV: vec2f,
  @location(1) fragPosition: vec4f
) -> @location(0) vec4f {
  return textureSample(myTexture, mySampler, fragUV) * fragPosition;
}