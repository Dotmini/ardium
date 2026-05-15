#include <metal_stdlib>
using namespace metal;

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - SHADERS
 * ============================================================================
 *  High-Performance Shader Library for 2D Primitives.
 * ============================================================================
 */

// Matches Ardium::Graphics::Vertex
struct VertexIn {
    float2 position;
    float4 color;
};

// Output passed from Vertex -> Fragment
struct RasterizerData {
    float4 position [[position]];
    float4 color;
    float2 uv; // For circle rendering
};

// --- CONSTANTS ---
constant float2 QUAD_VERTICES[6] = {
    float2(-1.0, -1.0), // Bottom Left
    float2( 1.0, -1.0), // Bottom Right
    float2(-1.0,  1.0), // Top Left
    float2( 1.0, -1.0), // Bottom Right
    float2( 1.0,  1.0), // Top Right
    float2(-1.0,  1.0)  // Top Left
};

/**
 * @brief Instanced Vertex Shader
 * Draws generic 2D Quads/Points based on instance data.
 */
vertex RasterizerData basic_vertex(uint vertexID [[vertex_id]],
                                   uint instanceID [[instance_id]],
                                   const device VertexIn* instances [[buffer(0)]],
                                   constant float2& viewportSize [[buffer(1)]]) {
    
    RasterizerData out;
    
    // Get per-instance data
    VertexIn instance = instances[instanceID];
    
    // Base Geometry (Quad)
    float2 basePos = QUAD_VERTICES[vertexID];
    
    // Transform: Scale (Fixed for now, e.g., 5px radius) + Translate
    float pointSize = 5.0; 
    float2 worldPos = (basePos * pointSize) + instance.position;
    
    // Normalize to NDC (-1 to 1)
    // 0,0 is Top-Left in Ardium -> Metal -1,1
    float x = (worldPos.x / viewportSize.x) * 2.0 - 1.0;
    float y = (1.0 - (worldPos.y / viewportSize.y)) * 2.0 - 1.0;
    
    out.position = float4(x, y, 0.0, 1.0);
    out.color = instance.color;
    out.uv = basePos; // -1 to 1 range
    
    return out;
}

/**
 * @brief Fragment Shader - Circle
 * Discards pixels outside the unit circle radius to draw round points.
 */
fragment float4 circle_fragment(RasterizerData in [[stage_in]]) {
    // Calculate distance from center (0,0) in UV space
    float distSq = dot(in.uv, in.uv);
    
    // Hard edge circle (change 1.0 to something lower for anti-aliasing logic)
    if (distSq > 1.0) {
        discard_fragment();
    }
    
    return in.color;
}

/**
 * @brief Fragment Shader - Rect
 * Simple pass-through color.
 */
fragment float4 rect_fragment(RasterizerData in [[stage_in]]) {
    return in.color;
}