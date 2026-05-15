using namespace metal;

struct Particle {
    float x; float y;
    float vx; float vy;
    float r; float g; float b; float a;
};

struct VertexOut {
    float4 position [[position]];
    float4 color;
    float pointSize [[point_size]];
};

vertex VertexOut particle_vertex(uint vertexID [[vertex_id]],
                                 const device Particle* particles [[buffer(0)]],
                                 constant float2& viewportSize [[buffer(1)]]) {
    VertexOut out;
    Particle p = particles[vertexID];
    
    // Map 0..Width/Height to -1..1 NDC
    // Invert Y because Metal coords are Y-up in NDC (bottom-left -1,-1) but we use Y-down logic usually?
    // Actually Metal NDC is (-1, -1) to (1, 1). 
    // Standard 2D: (0,0) Top-Left. 
    float x = (p.x / viewportSize.x) * 2.0 - 1.0;
    float y = (1.0 - (p.y / viewportSize.y)) * 2.0 - 1.0; // Flip Y for Top-Left origin
    
    out.position = float4(x, y, 0.0, 1.0);
    out.color = float4(p.r, p.g, p.b, p.a);
    out.pointSize = 2.0; // Small points for 1M particles
    return out;
}

fragment float4 particle_fragment(VertexOut in [[stage_in]]) {
    return in.color;
}
