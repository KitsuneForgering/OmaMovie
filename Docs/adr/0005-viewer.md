## Implementation in the viewer (2026-10-03)

The viewer composites on Qt's render thread, in `updatePaintNode` during sync,
and Qt samples the compositor's display image through `QRhiTexture::createFrom`
(`apps/omamovie/src/preview_item.*`). This meets the admission requirement by
construction for the viewer: the thread that submits the compositor's work is
the one that creates, resizes and destroys the swapchain, so they never overlap
Qt's `vkDeviceWaitIdle`.

- Sync runs inside the `beforeFrameBegin`…`afterFrameEnd` interval in which the
  bridge holds the graphics queue, so `gpu::Device`'s queue lock is now
  recursive: the render thread re-enters it to submit; other threads still wait.
- The compositor submits on the same graphics queue as Qt. Its next display
  encode begins with an `ALL_COMMANDS` barrier, so it waits for Qt's earlier
  sampling; Qt's sampling is submitted after the encode and sees its writes.
- A display-size change waits for the queue to go idle before the old image is
  destroyed; the scene-graph node owns Qt's texture wrapper, and the item
  destroys the compositor on `sceneGraphInvalidated` after the queue is idle.
- The display image is RGBA8 encoded with the sRGB transfer function
  (`encode_display`); ADR-0006 still owns the color policy.
- Validation: the GUI smoke run under the Khronos validation layer with
  synchronization validation reports no errors when no window capture is
  taken. `QQuickWindow::grabWindow` (screenshots only) triggers swapchain
  present/acquire hazards in Qt itself, with or without the viewer compositing.
- Decoding stays in software for now (one plane upload per frame): hardware
  frames come from a decode thread and need the admission protocol above.

[Return to 0005-qt-vulkan-interop.md](0005-qt-vulkan-interop.md)
