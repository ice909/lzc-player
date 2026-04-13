export type PlayerEventName =
  | 'state'
  | 'progress'
  | 'tracks'
  | 'error'
  | 'snapshot'
  | 'playing'
  | 'timePos'
  | 'duration'
  | 'playbackSpeed'
  | 'volume'
  | 'qualityLabel'
  | 'subtitleTracks'
  | 'subtitleId'
  | 'videoFps';

export interface PlayerError {
  code?: string;
  message: string;
  [key: string]: unknown;
}

export interface SubtitleTrack {
  id: number;
  title: string;
  [key: string]: unknown;
}

export interface PlayerState {
  sourceUrl: string;
  playing: boolean;
  timePos: number;
  duration: number;
  playbackSpeed: number;
  volume: number;
  qualityLabel: string;
  subtitleTracks: SubtitleTrack[];
  subtitleId: number;
  videoFps: number;
  lastError: PlayerError | null;
}

export interface OpenPayload {
  url: string;
  headers?: Record<string, string>;
  cookie?: string;
  title?: string;
  startPaused?: boolean;
}

export interface Transport {
  request<T = unknown>(name: string, payload?: unknown): Promise<T>;
  subscribe(listener: (message: unknown) => void): () => void;
}

export interface ElectronBridge {
  invoke<T = unknown>(name: string, payload?: unknown): Promise<T>;
  subscribe(listener: (message: unknown) => void): () => void;
}

export interface PlayerClientOptions {
  initialState?: Partial<PlayerState>;
}

export interface MockTransport extends Transport {
  emit(name: PlayerEventName | string, payload: unknown): void;
  getState(): PlayerState;
}

export declare const EVENT_NAMES: Readonly<{
  STATE: 'state';
  PROGRESS: 'progress';
  TRACKS: 'tracks';
  ERROR: 'error';
  SNAPSHOT: 'snapshot';
}>;

export declare class LzcPlayerClient {
  constructor(transport: Transport, options?: PlayerClientOptions);
  getState(): PlayerState;
  subscribe(
    listener: (state: PlayerState) => void,
    options?: { emitCurrent?: boolean }
  ): () => void;
  on(
    eventName: PlayerEventName | string,
    listener: (payload: unknown, state: PlayerState) => void
  ): () => void;
  open(payload: OpenPayload): Promise<unknown>;
  close(): Promise<unknown>;
  togglePause(): Promise<unknown>;
  seekTo(seconds: number): Promise<unknown>;
  seekRelative(seconds: number): Promise<unknown>;
  setVolume(volume: number): Promise<unknown>;
  setPlaybackSpeed(speed: number): Promise<unknown>;
  setSubtitleId(id: number): Promise<unknown>;
  send(command: string, payload?: unknown): Promise<unknown>;
  destroy(): void;
}

export declare function createLzcPlayerClient(
  transport: Transport,
  options?: PlayerClientOptions
): LzcPlayerClient;

export declare function createElectronBridgeTransport(bridge: ElectronBridge): Transport;

export declare function createElectronRendererPlayer(
  bridge: ElectronBridge,
  options?: PlayerClientOptions
): LzcPlayerClient;

export declare function createMockTransport(initialState?: Partial<PlayerState>): MockTransport;
