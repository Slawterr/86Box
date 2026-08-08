# Creative DXR2

## Project objective

Implement the Creative PC-DVD Encore Dxr2 MPEG-2 decoder as an emulated PCI expansion card in 86Box.

The implementation must work with the original, unmodified Creative Windows drivers and original Creative playback software. No custom guest driver, replacement guest codec, compatibility shim, or patched Creative binary is acceptable.

The initial target should be one precisely identified board and software combination, preferably a Creative CT7120 or CT7220 with its matching Windows 98 driver and PC-DVD player release.

## Emulation boundary

The guest-visible hardware interface must be emulated accurately:

```text
Original Creative driver and player
                |
                v
Emulated PCI configuration, registers, DMA, FIFOs and interrupts
                |
                v
Host-side MPEG decoding through FFmpeg libraries
                |
                v
Emulated Dxr2 video overlay/output and audio output
```

FFmpeg may replace the internal mathematical decoding performed by the C-Cube ZiVA chip. It must not replace or bypass the interface used by the original Creative driver.

## Known board components

- Auravision VXP524 PCI/video processor
- C-Cube ZiVA DS-L MPEG-1/MPEG-2 decoder
- Brooktree Bt865 video encoder
- Approximately 1.25 MB plus 256 KB of onboard DRAM
- VGA input/output loop-through
- Composite and S-Video outputs
- Audio input/output with MPEG audio and Dolby Digital support

Exact components, PCI IDs and memory sizes must be confirmed for the selected board revision.

## Required guest-visible behavior

- Exact PCI vendor, device, revision and subsystem IDs
- Correct PCI BAR types, sizes and reset mappings
- Hardware reset and power-on register values
- VXP524 register behavior
- Onboard-memory access and diagnostics
- ZiVA initialization and any firmware or table uploads
- DMA descriptors, input FIFOs and buffer status
- Interrupt generation, masking and acknowledgement
- Playback clock and presentation timestamps
- Play, pause, stop, seek, flush and underrun behavior
- MPEG video, audio and subpicture stream handling
- Overlay position, scaling, aspect ratio and color-key controls
- VGA loop-through and television-output behavior
- Save-state support once the device is stable

## FFmpeg integration

Prefer library integration rather than launching an external `ffmpeg` process:

- `libavcodec` for MPEG-1/MPEG-2 video, MPEG audio and AC-3
- `libavformat`, or a small internal parser, for MPEG program streams and PES packets
- `libswscale` for YUV conversion and scaling when required
- `libswresample` for audio conversion when required

FFmpeg processing must be paced according to the emulated decoder clock. Guest submissions must not complete instantaneously if the real driver expects FIFO pressure, decoder-busy states, completion interrupts or timed presentation.

The build integration should initially be optional. The selected FFmpeg configuration and distribution method must be reviewed for compatibility with 86Box licensing and cross-platform packaging.

## Development plan

### Phase 0: preserve the reference environment

- Obtain an original Creative driver and PC-DVD player package.
- Record exact versions and hashes.
- Select one CT7120 or CT7220 revision as the first target.
- Preserve sample unencrypted MPEG-2 program streams.
- Obtain the original diagnostics and overlay-calibration utilities if available.
- Recover the historical open-source Linux Dxr2 driver and related headers.

### Phase 1: PCI skeleton

- Register the card in the 86Box device selection system.
- Implement PCI configuration space and correct identification.
- Implement BAR allocation, interrupt routing and reset state.
- Provide logged placeholders for unknown register accesses.

Acceptance criterion: the original Windows driver identifies the expected Creative device and begins initialization.

### Phase 2: driver initialization

- Analyze the original driver for register reads and writes, polling loops, timeouts, DMA structures and firmware transfers.
- Implement register readback and reset sequencing.
- Implement onboard-memory access sufficiently for driver diagnostics.
- Implement firmware or initialization-table upload paths.
- Implement status and interrupt acknowledgement.

Acceptance criterion: Windows Device Manager reports that the original driver started successfully without a device error.

### Phase 3: compressed-stream transport

- Implement the real DMA or FIFO submission mechanism.
- Reconstruct descriptor formats and buffer ownership.
- Separate video, audio, subpicture and control packets when the hardware interface requires it.
- Model FIFO fullness, decoder-ready state, underruns and completion interrupts.
- Capture submitted MPEG/PES data for controlled comparison.

Acceptance criterion: the original Creative player opens the device and continuously submits a known unencrypted MPEG-2 stream.

### Phase 4: decoded video

- Connect the recovered stream to FFmpeg.
- Decode MPEG-2 sequence headers, pictures and interlaced fields.
- Respect presentation timestamps, frame ordering and aspect ratio.
- Initially render to a dedicated Dxr2 output or monitor.

Acceptance criterion: the original Creative player displays a stable unencrypted MPEG-2 video stream.

### Phase 5: audio and synchronization

- Decode MPEG audio and AC-3 as required.
- Add Dxr2-specific audio routing, mute and volume controls.
- Synchronize audio and video to the emulated decoder clock.
- Implement pause, resume, seek and flush without stale frames or audio.

Acceptance criterion: sustained synchronized playback under the original Creative software.

### Phase 6: VGA overlay and television output

- Implement color-keyed VGA loop-through composition.
- Implement overlay placement, scaling and fullscreen modes.
- Expose composite/S-Video behavior as a separate emulated monitor where appropriate.
- Model NTSC/PAL selection and the observable Bt865 controls needed by the driver.

Acceptance criterion: the original overlay-calibration and playback controls behave as expected.

### Phase 7: DVD-specific behavior

- Add DVD subpicture decoding and composition.
- Verify menus, highlights, multiple audio streams and chapter transitions.
- Investigate region and drive-authentication behavior.
- Treat CSS-encrypted media as a separate legal, dependency and implementation decision.

Unencrypted DVD images and MPEG files take priority over encrypted commercial discs.

## Reverse-engineering plan

Use evidence in this order:

1. Original Creative Windows driver behavior
2. Register and DMA traces from a physical Dxr2
3. Historical open-source Linux Dxr2 driver
4. Original Creative player and diagnostic behavior
5. Board manuals, photographs and component documentation

Useful static-analysis targets include:

- INF hardware and subsystem IDs
- Driver entry and PCI-resource initialization
- Port and MMIO access helpers
- Reset polling loops
- DMA descriptor builders
- Interrupt service routine
- IOCTL dispatch used by the player
- Firmware and table files in the installation package

Useful physical captures include:

- PCI configuration-space dump
- Power-on and post-driver register values
- Initialization access trace
- DMA descriptors and payload boundaries
- Interrupt/status sequences during start, pause, seek and stop
- Overlay-calibration register changes

## Initial acceptance target

A clean Windows 98 installation must:

1. Detect the emulated Dxr2.
2. Install the original Creative driver from original media without modification.
3. Report the device working in Device Manager.
4. Run the original Creative PC-DVD player.
5. Play an unencrypted MPEG-2 program stream with synchronized video and audio.

## Explicit non-goals for the first version

- Custom or modified guest drivers
- Replacement Windows codecs
- Patched Creative player binaries
- Cycle-accurate emulation of the internal ZiVA decoding pipeline
- Every Dxr2 board and driver revision
- Dxr3 or Sigma EM8300 compatibility
- CSS support before unencrypted playback works
- Perfect analog signal degradation

## Risks

- Original driver packages may differ substantially by board revision.
- Undocumented initialization may require physical hardware traces.
- Driver memory tests may expose details not visible in public documentation.
- Overlay composition crosses the boundary between a PCI multimedia device and the selected primary video card.
- DVD navigation and subpictures are separate from basic MPEG-2 decoding.
- CSS and region behavior require an explicit legal and packaging review.
- FFmpeg dependencies may complicate Windows, Linux and macOS distribution.

## Effort estimate

These are research estimates rather than schedules:

- PCI detection: days
- Original driver starts: several weeks
- Original player submits usable MPEG data: one to two months
- Stable MPEG-2 video and synchronized audio: two to four months total
- Broad DVD and driver-revision compatibility: follow-up work

## Reference starting points

- Creative PC-DVD Encore/Dxr2 manuals and original installation media
- Historical Dxr2 Resource Center driver source and mailing-list archives
- MPlayer `vo_dxr2` output implementation and Dxr2 userspace headers
- Auravision VXP524, C-Cube ZiVA DS-L and Brooktree Bt865 documentation
- 86Box PCI, DMA, audio, monitor and Voodoo pass-through implementations
- FFmpeg `libavcodec`, `libavformat`, `libswscale` and `libswresample` documentation
