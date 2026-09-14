# Baseline: corridor, M1, 1080p -- Lumen with hardware ray tracing

Captured 2026-09-14, after enabling r.RayTracing, SM6 and the skin cache (commit 94f9fa5).
File: 2026-09-14_corridor_M1_1080p_LumenHWRT.csv
Compare against: 2026-09-14_corridor_M1_1080p.csv (same conditions, Lumen was silently off)

## Conditions
Identical to the no-GI capture except: 3000 frames captured, first 900 discarded (2100 analysed),
to give the Lumen scene time to converge.
- Standalone -game, 1920x1080 windowed, VSync off, uncapped, editor closed, Epic scalability
- RHI: D3D12 at SM6. Log: "Ray tracing is enabled (dynamic)", "Ray tracing shaders are enabled"
- View: PlayerStart (160, 0, 95) looking down corridor A, standing still

## Results vs no-GI (avg ms)
| stat             | no GI | Lumen HWRT | change |
|------------------|-------|------------|--------|
| FrameTime        | 5.30  | 8.14       | +2.84  |
| GameThreadTime   | 1.84  | 2.08       | +0.24  |
| RenderThreadTime | 5.29  | 8.13       | +2.84  |
| RHIThreadTime    | 1.62  | 3.35       | +1.73  |
| GPUTime          | 4.94  | 6.92       | +1.98  |

- No GI:      189 fps avg, 1% low 157 fps, p99 6.35 ms, worst 14.81 ms
- Lumen HWRT: 123 fps avg, 1% low 89 fps,  p99 11.30 ms, worst 20.82 ms
- Average frame uses about half of the 16.67 ms budget for 60 fps.

## Proof Lumen is active
Present now, absent before: LumenReflections 0.186, LumenSceneUpdate 0.095, RayTracingScene 0.076,
LumenScreenProbeGather 0.005.
Present before, gone now: SSAO 0.088, SSAOSetup 0.010, ScreenSpaceReflections 0.023,
ReflectionEnvironment 0.074.

## OPEN QUESTION -- not yet confirmed
LumenScreenProbeGather (the diffuse bounce-light pass) measured only 0.005 ms. It is normally one of
the most expensive Lumen passes, so either its cost is booked under another GPU stat (Lights rose
0.46 -> 1.55 ms, RenderDeferredLighting 0.09 -> 0.41 ms) or diffuse GI is not doing full work.
No Lumen warnings in the log; r.Lumen.DiffuseIndirect.Allow=1. To be settled by a visual A/B
toggling r.Lumen.DiffuseIndirect.Allow 0/1, or a measured capture with it forced to 0.

## Top GPU passes (avg ms)
1.55 Lights, 0.95 TemporalSuperResolution, 0.90 Basepass, 0.56 VolumetricFog, 0.52 ShadowDepths,
0.41 RenderDeferredLighting, 0.33 Unaccounted, 0.25 ShadowProjection, 0.23 Postprocessing,
0.19 LumenReflections, 0.18 VolumetricCloud, 0.14 NaniteVisBuffer.
