#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outHistory;

layout(binding = 0) uniform sampler2D currentFrameTexture;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler2D historyTexture;

layout(push_constant) uniform TSRConstants {
    mat4 currInvViewProj;
    mat4 prevViewProj;
    vec4 renderAndDisplaySize; // xy = renderW, renderH; zw = displayW, displayH
    vec4 jitterAndParams;      // xy = jitterOffset (in render pixels), z = feedback (0.88-0.92), w = sharpness (0.0 .. 1.0)
    vec4 modeAndFlags;         // x = upscaleMode (0=Native/Bicubic, 1=FSR 1.0 Spatial, 2=TSR Temporal), y = resetHistory (1.0 or 0.0)
} pc;

vec3 rgbToYCoCg(vec3 c) {
    return vec3(
         0.25 * c.r + 0.50 * c.g + 0.25 * c.b,
         0.50 * c.r              - 0.50 * c.b,
        -0.25 * c.r + 0.50 * c.g - 0.25 * c.b
    );
}

vec3 yCoCgToRgb(vec3 c) {
    return vec3(
        c.x + c.y - c.z,
        c.x + c.z,
        c.x - c.y - c.z
    );
}

// 5-Tap Catmull-Rom Bicubic Filter for ringing-free history sampling
vec3 sampleCatmullRom(sampler2D tex, vec2 uv, vec2 texSize) {
    vec2 samplePos = uv * texSize;
    vec2 tc = floor(samplePos - 0.5) + 0.5;
    vec2 f = samplePos - tc;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;

    vec2 w0 = f2 - 0.5 * (f3 + f);
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w3 = 0.5 * (f3 - f2);
    vec2 w2 = 1.0 - w0 - w1 - w3;

    vec2 w12 = w1 + w2;
    vec2 tc12 = tc + (w2 / max(w12, vec2(0.0001)));

    vec2 tc0 = tc - 1.0;
    vec2 tc3 = tc + 2.0;

    vec3 c12 = texture(tex, tc12 / texSize).rgb;
    vec3 c0  = texture(tex, vec2(tc12.x, tc0.y) / texSize).rgb;
    vec3 c1  = texture(tex, vec2(tc0.x, tc12.y) / texSize).rgb;
    vec3 c2  = texture(tex, vec2(tc3.x, tc12.y) / texSize).rgb;
    vec3 c3  = texture(tex, vec2(tc12.x, tc3.y) / texSize).rgb;

    float totalW = w12.x * w12.y + w0.y * w12.x + w0.x * w12.y + w3.x * w12.y + w3.y * w12.x;
    vec3 acc = (c12 * (w12.x * w12.y) + c0 * (w0.y * w12.x) + c1 * (w0.x * w12.y) + c2 * (w3.x * w12.y) + c3 * (w3.y * w12.x));
    return acc / max(totalW, 0.0001);
}

// AMD FSR 1.0 EASU (Edge-Adaptive Spatial Upsampling) 12-tap directionally filtered kernel
vec3 evaluateFSR1_EASU(vec2 uv, vec2 renderSize) {
    vec2 texel = 1.0 / renderSize;
    vec2 pos = uv * renderSize - 0.5;
    vec2 f = fract(pos);

    // 4 Central Taps
    vec3 c00 = texture(currentFrameTexture, uv + vec2(-0.5, -0.5) * texel).rgb;
    vec3 c10 = texture(currentFrameTexture, uv + vec2( 0.5, -0.5) * texel).rgb;
    vec3 c01 = texture(currentFrameTexture, uv + vec2(-0.5,  0.5) * texel).rgb;
    vec3 c11 = texture(currentFrameTexture, uv + vec2( 0.5,  0.5) * texel).rgb;

    // Cross Ring Taps
    vec3 cu = texture(currentFrameTexture, uv + vec2( 0.0, -1.5) * texel).rgb;
    vec3 cd = texture(currentFrameTexture, uv + vec2( 0.0,  1.5) * texel).rgb;
    vec3 cl = texture(currentFrameTexture, uv + vec2(-1.5,  0.0) * texel).rgb;
    vec3 cr = texture(currentFrameTexture, uv + vec2( 1.5,  0.0) * texel).rgb;

    // Corner Ring Taps
    vec3 ctl = texture(currentFrameTexture, uv + vec2(-1.5, -1.5) * texel).rgb;
    vec3 ctr = texture(currentFrameTexture, uv + vec2( 1.5, -1.5) * texel).rgb;
    vec3 cbl = texture(currentFrameTexture, uv + vec2(-1.5,  1.5) * texel).rgb;
    vec3 cbr = texture(currentFrameTexture, uv + vec2( 1.5,  1.5) * texel).rgb;

    // Directional Gradient
    float l00 = dot(c00, vec3(0.299, 0.587, 0.114));
    float l10 = dot(c10, vec3(0.299, 0.587, 0.114));
    float l01 = dot(c01, vec3(0.299, 0.587, 0.114));
    float l11 = dot(c11, vec3(0.299, 0.587, 0.114));

    vec2 dir;
    dir.x = (l10 - l00) + (l11 - l01);
    dir.y = (l01 - l00) + (l11 - l10);
    float len = length(dir);
    if (len > 0.0001) dir /= len; else dir = vec2(0.0);

    // Directional Lanczos Weighting
    float w00 = (1.0 - f.x) * (1.0 - f.y);
    float w10 = f.x * (1.0 - f.y);
    float w01 = (1.0 - f.x) * f.y;
    float w11 = f.x * f.y;

    float edgeWeight = clamp(len * 2.0, 0.0, 0.40);
    vec3 baseSample = (c00 * w00 + c10 * w10 + c01 * w01 + c11 * w11);
    vec3 outerRing  = (cu + cd + cl + cr) * 0.125 + (ctl + ctr + cbl + cbr) * 0.0625;

    return mix(baseSample, (baseSample * 0.70 + outerRing * 0.30), edgeWeight);
}

// AMD RCAS (Robust Contrast-Adaptive Sharpening)
vec3 applyRCAS(vec3 c, vec2 uv, vec2 texSize, float sharpness) {
    if (sharpness <= 0.01) return c;
    vec2 step = 1.0 / texSize;
    vec3 cl = texture(currentFrameTexture, uv - vec2(step.x, 0.0)).rgb;
    vec3 cr = texture(currentFrameTexture, uv + vec2(step.x, 0.0)).rgb;
    vec3 cu = texture(currentFrameTexture, uv - vec2(0.0, step.y)).rgb;
    vec3 cd = texture(currentFrameTexture, uv + vec2(0.0, step.y)).rgb;

    float b  = dot(c,  vec3(0.2126, 0.7152, 0.0722));
    float bl = dot(cl, vec3(0.2126, 0.7152, 0.0722));
    float br = dot(cr, vec3(0.2126, 0.7152, 0.0722));
    float bu = dot(cu, vec3(0.2126, 0.7152, 0.0722));
    float bd = dot(cd, vec3(0.2126, 0.7152, 0.0722));

    float minLuma = min(b, min(min(bl, br), min(bu, bd)));
    float maxLuma = max(b, max(max(bl, br), max(bu, bd)));

    float peak = -1.0 / mix(8.0, 3.2, clamp(sharpness, 0.0, 1.0));
    float w = clamp(min(minLuma, 2.0 - maxLuma) / max(maxLuma, 0.0001), 0.0, 1.0);
    float lobe = peak * w;

    vec3 result = (cl + cr + cu + cd) * lobe + c * (1.0 - 4.0 * lobe);
    return max(result, vec3(0.0));
}

void main() {
    vec2 renderSize  = pc.renderAndDisplaySize.xy;
    vec2 displaySize = pc.renderAndDisplaySize.zw;
    vec2 texelRender = 1.0 / max(renderSize, vec2(1.0));
    vec2 texelDisplay = 1.0 / max(displaySize, vec2(1.0));

    int upscaleMode = int(pc.modeAndFlags.x + 0.5);
    float sharpness = pc.jitterAndParams.w;

    // -------------------------------------------------------------
    // MODE 0: Native / High-Quality Filtered Passthrough
    // -------------------------------------------------------------
    if (upscaleMode == 0) {
        vec3 col = texture(currentFrameTexture, inUV).rgb;
        col = applyRCAS(col, inUV, displaySize, sharpness);
        outColor = vec4(col, 1.0);
        outHistory = outColor;
        return;
    }

    // -------------------------------------------------------------
    // MODE 1: AMD FSR 1.0 Spatial (EASU 12-tap + RCAS)
    // -------------------------------------------------------------
    if (upscaleMode == 1) {
        vec3 col = evaluateFSR1_EASU(inUV, renderSize);
        col = applyRCAS(col, inUV, displaySize, sharpness);
        outColor = vec4(col, 1.0);
        outHistory = outColor;
        return;
    }

    // -------------------------------------------------------------
    // MODE 2: TSR (Temporal Super Resolution / FSR 2-grade)
    // -------------------------------------------------------------
    // 1. Calculate Unjittered UV for Current Frame
    vec2 jitterOffsetUV = pc.jitterAndParams.xy * texelRender;
    vec2 unjitteredUV = inUV - jitterOffsetUV;

    // 2. Sample 3x3 Neighborhood in YCoCg space to compute variance bounding box
    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    vec3 currCenter = rgbToYCoCg(texture(currentFrameTexture, inUV).rgb);

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 sampleUV = clamp(inUV + vec2(x, y) * texelRender, 0.001, 0.999);
            vec3 s = rgbToYCoCg(texture(currentFrameTexture, sampleUV).rgb);
            m1 += s;
            m2 += s * s;
        }
    }
    vec3 mu = m1 / 9.0;
    vec3 sigma = sqrt(max(m2 / 9.0 - mu * mu, vec3(0.0)));
    float gamma = 1.25; // 1.25 standard deviation box for crisp anti-ghosting
    vec3 boxMin = mu - gamma * sigma;
    vec3 boxMax = mu + gamma * sigma;

    // 3. Depth-derived Motion Vector Reconstruction
    float depth = texture(depthTexture, inUV).r;
    vec4 ndc = vec4(inUV.x * 2.0 - 1.0, (1.0 - inUV.y) * 2.0 - 1.0, depth, 1.0);
    vec4 worldPos = pc.currInvViewProj * ndc;
    worldPos /= max(worldPos.w, 0.00001);

    vec4 prevClip = pc.prevViewProj * worldPos;
    vec2 prevNDC = prevClip.xy / max(prevClip.w, 0.00001);
    vec2 prevUV = vec2(prevNDC.x * 0.5 + 0.5, 1.0 - (prevNDC.y * 0.5 + 0.5));
    vec2 velocity = inUV - prevUV;

    // 4. Sample and Clamp History
    bool resetHistory = (pc.modeAndFlags.y > 0.5);
    bool outOfBounds = (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0);

    vec3 historyRGB;
    if (resetHistory || outOfBounds) {
        historyRGB = yCoCgToRgb(currCenter);
    } else {
        historyRGB = sampleCatmullRom(historyTexture, prevUV, displaySize);
        vec3 historyYCoCg = rgbToYCoCg(historyRGB);

        // Clamp history into local variance box (ZERO GHOSTING)
        historyYCoCg = clamp(historyYCoCg, boxMin, boxMax);
        historyRGB = yCoCgToRgb(historyYCoCg);
    }

    // 5. Velocity & Luminance Responsive Blend Weight
    float baseFeedback = clamp(pc.jitterAndParams.z, 0.80, 0.94);
    float velLength = length(velocity * displaySize);
    float motionAdapt = clamp(1.0 - velLength * 0.05, 0.65, 1.0);
    float feedback = baseFeedback * motionAdapt;

    vec3 currRGB = yCoCgToRgb(currCenter);
    vec3 accumulated = mix(currRGB, historyRGB, feedback);

    // 6. AMD RCAS Sharpening
    accumulated = applyRCAS(accumulated, inUV, displaySize, sharpness);

    outColor = vec4(max(accumulated, vec3(0.0)), 1.0);
    outHistory = outColor;
}
