# HermesX UI components

This directory contains HermesX-specific UI policy and feature components that
are independent from the upstream `Screen` lifecycle.

## Boundaries

- `Screen` remains the only observer of device `InputEvent` objects.
- `HermesXUiInputRouter` decides which visible UI layer owns an input event.
- The selected feature controller handles the event and reports whether it was
  consumed.
- Rendering order and input priority must use the same top-to-bottom order.
- UI components may own cursors, drafts, view state, and drawing code.
- Persistent preferences, Mesh sends, database changes, and global config
  mutation belong behind module/service APIs rather than renderer code.

The first service boundary is `HermesXPreferences`; new UI preferences should
use it instead of opening `/prefs` files directly from renderer functions.

## Planned feature slices

1. Message list, detail, popup, and composer UI. Recent-message and popup state
   live in `HermesXMessageUiModel`; input interpretation and state transitions
   live in `HermesXMessageUiController`. Recent Send list, incoming popup, and
   detail-page drawing live in `HermesXMessageUiRenderer`.
   The message controller also owns incoming-popup activation/timeout and the
   post-dismiss palette-recovery request shared by direct Home/GPS UI-skip
   policy.
   `HermesXDirectMessageComposer` owns the direct-message draft, keyboard cursor,
   Bopomofo candidates, and input transitions. The message renderer draws the
   candidate page, while the shared FastSetup renderer draws its keyboard.
   `Screen` retains Mesh send and page-navigation side effects.
2. TraceRoute list, detail, binding, search, and popup UI.
   `HermesXTraceRouteUiModel` now owns popup and pending-request identity;
   `HermesXTraceRouteUiController` owns popup dismiss/scroll transitions, and
   `HermesXTraceRouteUiRenderer` draws the overlay. The model/controller also
   own ShortName search draft, keyboard cursor, result selection, and input
   transitions. TraceRoute menu, online-binding list, confirmation dialog,
   Quick Route list cursors, and deferred rotary short-press state now live in
   the model/controller as well. `HermesXNodeBrowserDataSource` owns the shared
   online-node filtering, ordering, ShortName search, and NodeNum lookup.
   `Screen` retains page switching and Mesh send side effects. The renderer
   now draws the TraceRoute menu, online-binding list, confirmation dialog, and
   Quick Route list through read-only row providers; it does not depend on
   NodeDB or persistence types. It also draws the full-page ShortName search
   result from display-only node names supplied by `Screen`. The search keyboard
   reuses `HermesXFastSetupUiRenderer::drawKeyboardPage()`. Persistent bound-node loading,
   deduplication, capacity enforcement, saving, binding, and unbinding live in
   `HermesXTraceRouteBindings`; `Screen` only maps UI actions to that service.
3. Online, Finder, and GROUP node browser UI. Shared list selection and detail
   cursor state now live in `HermesXNodeBrowserUiModel`;
   `HermesXNodeBrowserUiController` interprets menu, list, and detail input and
   reports navigation actions. `HermesXNodeBrowserUiRenderer` draws menus,
   lists, right-side status, empty states, and detail rows from display-only row
   providers. `HermesXNodeBrowserDataSource` owns Online/Finder NodeDB filtering,
   recency and distance ordering, and selection-to-node lookup. Finder pulse
   confirmation/sending state, arm timing, navigation, and input transitions
   live in the node-browser model/controller. The renderer owns the warning,
   sending overlay, animated radar, and shared Finder radar icon. Lighthouse
   request/cancel/result side effects, GROUP configuration, and page switching
   remain in `Screen`.
4. FastSetup UI backed by settings service APIs. The entry page, Root menu,
   shared header, list, toast, entry-icon, and TFT palette drawing now live in
   `HermesXFastSetupUiRenderer`. `HermesXFastSetupUiModel` owns the active page,
   return page, selection, scroll offset, and navigation debounce state;
   `HermesXFastSetupUiController` owns wrapped list navigation and its timing
   guards. It also dispatches Root, Node, Canned, DeviceInfo, Power, GPS, UI,
   LoRa, EMAC, and EMINFO menu selections into page transitions or explicit
   side-effect requests. Update, MQTT, MQTT map-report, Channel, and Channel
   detail menus now use the same action-dispatch boundary; OTA, WiFi, MQTT,
   configuration mutation, and radio side effects remain in `Screen`.
   NodeDB cleanup, MQTT map precision/publish, and Channel precision selection
   pages return validated selection results from the controller. NodeDB, MQTT,
   MQTT map-report, Channel list/detail, and their selection-page rendering now
   live in the renderer behind display-only values. Power, LoRa, dynamic LoRa
   channel-slot, GPS, Canned, UI, GROUP PIN, EMAC, EMINFO, Node, DeviceInfo, and
   their selection-page rendering use the same boundary. Update entry, check,
   runtime, WiFi configuration, WiFi upload, and USB upload list rendering also
   use the renderer behind display-only values. Update transition, URL flow,
   upload-progress, and the shared FastSetup/MSG/TraceRoute keyboard rendering
   use the same boundary. The shared detail popup is also rendered from
   display-only title/body and referenced scroll bounds.
   `HermesXDetailPopupModel` owns its content, visibility, scroll bounds, and
   open timestamp; `HermesXDetailPopupController` owns the dismiss guard,
   scrolling, and close transition. Configuration changes and other side
   effects remain outside the renderer.
5. Home/GPS direct-TFT renderers and their cache state. Home quote ownership,
   wrapped quote drawing, the OLED fallback dog animation, horizontal battery,
   and the date/role/battery/satellite status rendering now live in
   `HermesXHomeUiRenderer` behind `HermesXHomeStatusView`.
   `HermesXHomeDirectRenderer` owns the direct-TFT dog sprite frames and
   diff-based dog painting.
   `HermesXHomeDirectPresenter` owns Direct Home entry/exit TFT clearing,
   palette transitions, quote start, compact-layout eligibility, UI
   reinitialization through a narrow callback, and controller-backed dog
   presentation.
   `HermesXHomeUiController` owns Direct Home entry/exit transitions,
   activation policy, animation timing/FPS, telemetry refresh interval,
   base-painted lifecycle, dog-pose rotation, redraw decisions, and palette
   invalidation routing.
   `HermesXHomeStateCollector` converts raw RTC, battery, GPS, stealth, and role
   samples into the Home state snapshot; it also owns Home battery fallback
   caching, percentage estimation, and time/date formatting without depending
   on `Screen` device globals.
   `HermesXHomeUiModel` owns the direct-TFT base-state snapshot, telemetry
   refresh timing, dog animation cache, dirty decisions, invalidation, and
   commit behavior. Shared Neon layout constants and layer-color mapping live
   in `HermesXHomeUiRenderer`. `HermesXNeonWorkspace` owns the GPS text scratch
   buffer, glyph-cache storage, allocation, release, lazy glyph mask/glow
   composition, text measurement/layout/layer assembly, and paint-run planning.
   `HermesXNeonRenderer` owns TFT background clearing, layer-color mapping,
   paint-run execution, and GPS text render
   orchestration. `HermesXDirectTftPrimitives` owns clipped rectangles, circles,
   lines, thick lines, and Neon line rasterization shared by Home and GPS.
   `HermesXPattanakarnNeonFont` owns Pattanakarn glyph lookup,
   measurement, OLED raster drawing, and Neon glyph-cache adaptation.
   Low-memory policy remains in `Screen`.
   GPS satellite icons, backdrop and Neon ASCII title, dynamic decor, shared
   poster/title geometry, title-mask generation and neon raster output,
   coordinate/altitude formatting and layout, fallback corner-map outline, and
   the standard OLED status layout now live in `HermesXGpsUiRenderer` behind
   display-only inputs and narrow shared-drawing callbacks.
   `HermesXGpsDirectPresenter` owns poster visibility transition effects,
   palette and full-screen entry clearing, UI reinitialization through a narrow
   callback, and UI-skip policy.
   `HermesXGpsStateCollector` converts raw GPS/config samples into the cached
   poster state plus coordinate and decor views. `HermesXGpsDirectRenderer`
   owns warm/cool/alert colors, Neon text adaptation, and decor/title/coordinate
   layer rendering order without depending on `GPSStatus` or global config.
   `HermesXGpsUiController` owns frame-index tracking, frame-change
   invalidation, fixed-GPS-frame eligibility, direct-poster visibility, and
   shared Neon workspace acquire/release policy.
   `HermesXNeonMask` owns mask glow/interior tests shared with Home.
   `HermesXGpsUiModel` owns the direct-poster state snapshot, cache comparison,
   invalidation, commit behavior, visibility transition, base-painted state,
   and full-frame-after-switch guard. Raw GPS/config sampling remains in
   `Screen`.
6. TAK Mode navigation state and input interpretation. `HermesXTakModeUiModel`
   owns the active TAK page, popup/settings cursors, scroll offsets, and
   transition timestamp. `HermesXTakModeUiController` interprets configured
   buttons and rotary input, performs page/cursor transitions, and returns
   explicit actions. `HermesXTakModeUiRenderer` draws the shield, status page,
   transition, popup, settings list, and channel list from display-only values
   and row providers. `Screen` retains profile persistence, radio/device
   configuration, emergency/Finder launch, reboot, and other hardware side
   effects.
7. Low-memory protection overlay. `HermesXLowMemoryUiModel` owns visibility,
   selection, suppression timing, heap measurements, and status copy.
   `HermesXLowMemoryUiController` interprets option navigation, dismiss, and
   cleanup requests. `HermesXLowMemoryUiRenderer` draws the protection status
   and options from read-only state. Heap thresholds, Neon-buffer policy,
   alerts, NodeDB cleanup, and recovery side effects remain in `Screen`.
8. Emergency confirmation overlay. `HermesXEmergencyConfirmUiModel` owns
   visibility, countdown, and the cancel-request latch consumed by
   `ButtonThread`. `HermesXEmergencyConfirmUiController` converts any supported
   navigation or activation input into cancellation, and
   `HermesXEmergencyConfirmUiRenderer` draws the warning and countdown.
   Emergency timing, tones, and activation remain in `ButtonThread`.
9. Rotary-lock overlay. `HermesXRotaryLockUiModel` owns the lock value, popup
   visibility, selection, and timeout timestamp. `HermesXRotaryLockUiController`
   interprets rotary and standard navigation, confirmation, and cancellation.
   `HermesXRotaryLockUiRenderer` draws the lock/unlock dialog from read-only
   state. `Screen` retains display wake/redraw side effects and the public API
   used by `ButtonThread` and the rotary input driver.

Each slice should preserve the centralized input ownership defined by the
router and be compiled successfully before the next slice is moved.

Shared UTF-8 truncation, mixed Chinese/ASCII wrapping, scaling, and visible-line
drawing live in `HermesXTextLayout`; feature renderers should reuse it rather
than adding local copies.
