# Map-only wheel zoom regression, macOS Metal

The wheel-zoom implementation is contained in GNXMapEngine. GNXMapEngine uses the existing `MouseScrolledEvent`, normalizes its 120-per-tick units, and eases the accumulated input over render frames. There are no GLFW types, headers, callback replacements, or direct GLFW links in GNXMapEngine. The underlying window backend quantizes incoming scroll deltas; this map-only change cannot recover precision already discarded there.

Target: 110°E/23°N, pitch 30°, starting distance 1,000,000 m, panel hidden. `GNX_MAP_SCROLL_TEST_FRAME=65` injects an existing engine scroll event through `MapApplication::OnEvent`. This is an event replay, not a physical mouse/trackpad test. Each tick now corresponds to a right-button drag of 10% of viewport height (previously 5%), giving a distance multiplier of `2^0.2` at default sensitivity. Each render frame consumes at most 0.075 ticks.

| Frame | Camera distance | Image |
| --- | ---: | --- |
| 64, before event | 1,000,000 m | [before.png](before.png) |
| 65, first frame | 989,656.656 m | [first_frame.png](first_frame.png) |
| 73, moving | 927,374.400 m | [in_motion.png](in_motion.png) |
| 95, nearing target | 870,660.553 m | [settled.png](settled.png) |

The -1 tick replay produced 31 strictly decreasing frame distances, with a maximum frame change of 1.0343% and final error of 0.01263% against `1,000,000 × 2^(-0.2) = 870,550.563 m`. The +1 tick replay increased distance monotonically to 1,148,649.873 m, within 0.00422% of `1,000,000 × 2^0.2 = 1,148,698.355 m`; its maximum frame change was 1.0452%. Both runs also matched the expected per-frame distance after accounting for pending input, within 0.000007%. Distances and pending input are retained in [zoom_in.csv](zoom_in.csv) and [zoom_out.csv](zoom_out.csv).

The zoom-out run completed with Metal API validation enabled and screenshot capture disabled. The zoom-in screenshot run completed with Metal API validation disabled: the unchanged underlying engine's color-only screenshot pass still enables depth writing and asserts when Metal validation is enabled. GNXMapEngine and TestEarthCameraAngles built successfully; `ctest -R '^EarthCameraAngles$'` passed. The four displayed screenshot frames were inspected for consistent target position and progressive zoom; existing terrain texture seams remain visible.

To repeat, run the built application with `GNX_MAP_PANEL=0 GNX_MAP_TARGET_LON=110 GNX_MAP_TARGET_LAT=23 GNX_MAP_EYE_DISTANCE=1000000 GNX_MAP_PITCH=30 GNX_MAP_SCROLL_TEST_FRAME=65 GNX_MAP_SCROLL_TEST_STEPS=-1`. The test exits at frame 95 and logs each frame distance as `[ScrollTest]`. Set `GNX_MAP_SCROLL_TEST_DIR` to an existing directory and `MTL_DEBUG_LAYER=0` to capture frames 64, 65, 67, 73, and 95. Use `GNX_MAP_SCROLL_TEST_STEPS=1 MTL_DEBUG_LAYER=1` without the screenshot directory for the validation-enabled zoom-out check.
