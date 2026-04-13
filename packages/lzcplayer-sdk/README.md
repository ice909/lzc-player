# `@lzcplayer/sdk`

Electron-friendly SDK wrapper for controlling an `lzcplayer` native player process.

This package does not talk to Qt or mpv directly. It wraps a transport layer that your Electron app provides, then exposes a stable player API for the renderer side.

## API shape

```ts
import { createElectronRendererPlayer } from "@lzcplayer/sdk";

const player = createElectronRendererPlayer(window.lzcPlayerBridge);

await player.open({
  url: "https://example.com/video.mp4",
  headers: {
    Cookie: "HC-Auth-Token=..."
  }
});

player.subscribe((state) => {
  console.log(state.timePos, state.duration, state.playing);
});

await player.seekTo(120);
await player.togglePause();
```

## Expected Electron preload bridge

The renderer-facing bridge is intentionally small:

```ts
type Bridge = {
  invoke(name: string, payload?: unknown): Promise<unknown>;
  subscribe(listener: (message: unknown) => void): () => void;
};
```

Example preload:

```ts
import { contextBridge, ipcRenderer } from "electron";

contextBridge.exposeInMainWorld("lzcPlayerBridge", {
  invoke(name: string, payload?: unknown) {
    return ipcRenderer.invoke("lzcplayer:command", { name, payload });
  },
  subscribe(listener: (message: unknown) => void) {
    const wrapped = (_event: unknown, message: unknown) => listener(message);
    ipcRenderer.on("lzcplayer:event", wrapped);
    return () => ipcRenderer.off("lzcplayer:event", wrapped);
  },
});
```

## Expected IPC messages from the native player layer

The SDK accepts any object, but the intended format is:

```json
{ "type": "event", "name": "progress", "payload": { "timePos": 12.3, "duration": 634.0, "playing": true } }
```

Other useful event names:

- `state`
- `playing`
- `timePos`
- `duration`
- `playbackSpeed`
- `volume`
- `qualityLabel`
- `subtitleTracks`
- `subtitleId`
- `videoFps`
- `error`

## Mock transport

For front-end integration before the native IPC exists:

```ts
import { createLzcPlayerClient, createMockTransport } from "@lzcplayer/sdk";

const transport = createMockTransport({ duration: 300 });
const player = createLzcPlayerClient(transport);
```
