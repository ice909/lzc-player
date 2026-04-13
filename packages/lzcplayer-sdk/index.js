'use strict';

const EVENT_NAMES = Object.freeze({
  STATE: 'state',
  PROGRESS: 'progress',
  TRACKS: 'tracks',
  ERROR: 'error',
  SNAPSHOT: 'snapshot',
});

function createInitialState(overrides) {
  return {
    sourceUrl: '',
    playing: false,
    timePos: 0,
    duration: 0,
    playbackSpeed: 1,
    volume: 100,
    qualityLabel: '',
    subtitleTracks: [],
    subtitleId: 0,
    videoFps: 0,
    lastError: null,
    ...(overrides || {}),
  };
}

function cloneState(state) {
  return {
    ...state,
    subtitleTracks: Array.isArray(state.subtitleTracks) ? [...state.subtitleTracks] : [],
    lastError: state.lastError ? { ...state.lastError } : null,
  };
}

function assertTransport(transport) {
  if (!transport || typeof transport.request !== 'function') {
    throw new TypeError('lzcplayer sdk requires a transport with a request(name, payload) function');
  }

  if (typeof transport.subscribe !== 'function') {
    throw new TypeError('lzcplayer sdk requires a transport with a subscribe(listener) function');
  }
}

function normalizeOpenPayload(payload) {
  if (!payload || typeof payload.url !== 'string' || payload.url.length === 0) {
    throw new TypeError('open() requires a non-empty url string');
  }

  return {
    url: payload.url,
    headers: payload.headers || undefined,
    cookie: payload.cookie || undefined,
    title: payload.title || undefined,
    startPaused: Boolean(payload.startPaused),
  };
}

function normalizeEvent(message) {
  if (!message || typeof message !== 'object') {
    return { type: 'event', name: EVENT_NAMES.ERROR, payload: { message: 'Invalid IPC message' } };
  }

  if (message.type === 'event' && typeof message.name === 'string') {
    return message;
  }

  if (typeof message.name === 'string') {
    return { type: 'event', name: message.name, payload: message.payload };
  }

  if (typeof message.type === 'string' && message.type !== 'event') {
    return message;
  }

  return { type: 'event', name: EVENT_NAMES.SNAPSHOT, payload: message };
}

function applyEventToState(state, event) {
  const next = cloneState(state);

  switch (event.name) {
    case EVENT_NAMES.SNAPSHOT:
    case EVENT_NAMES.STATE:
      Object.assign(next, event.payload || {});
      break;
    case EVENT_NAMES.PROGRESS:
      Object.assign(next, event.payload || {});
      break;
    case 'playing':
      next.playing = Boolean(event.payload);
      break;
    case 'timePos':
      next.timePos = Number(event.payload || 0);
      break;
    case 'duration':
      next.duration = Number(event.payload || 0);
      break;
    case 'playbackSpeed':
      next.playbackSpeed = Number(event.payload || 1);
      break;
    case 'volume':
      next.volume = Number(event.payload || 0);
      break;
    case 'qualityLabel':
      next.qualityLabel = typeof event.payload === 'string' ? event.payload : '';
      break;
    case 'subtitleTracks':
    case EVENT_NAMES.TRACKS:
      next.subtitleTracks = Array.isArray(event.payload) ? [...event.payload] : [];
      break;
    case 'subtitleId':
      next.subtitleId = Number(event.payload || 0);
      break;
    case 'videoFps':
      next.videoFps = Number(event.payload || 0);
      break;
    case EVENT_NAMES.ERROR:
      next.lastError = event.payload || { message: 'Unknown player error' };
      break;
    default:
      if (event.payload && typeof event.payload === 'object') {
        Object.assign(next, event.payload);
      }
      break;
  }

  return next;
}

class LzcPlayerClient {
  constructor(transport, options) {
    assertTransport(transport);
    this._transport = transport;
    this._state = createInitialState(options && options.initialState);
    this._stateListeners = new Set();
    this._eventListeners = new Map();
    this._unsubscribeTransport = transport.subscribe((message) => this._handleMessage(message));
  }

  getState() {
    return cloneState(this._state);
  }

  subscribe(listener, options) {
    if (typeof listener !== 'function') {
      throw new TypeError('subscribe(listener) expects a function');
    }

    this._stateListeners.add(listener);
    if (!options || options.emitCurrent !== false) {
      listener(this.getState());
    }

    return () => {
      this._stateListeners.delete(listener);
    };
  }

  on(eventName, listener) {
    if (typeof eventName !== 'string' || eventName.length === 0) {
      throw new TypeError('on(eventName, listener) expects a non-empty event name');
    }
    if (typeof listener !== 'function') {
      throw new TypeError('on(eventName, listener) expects a function listener');
    }

    const listeners = this._eventListeners.get(eventName) || new Set();
    listeners.add(listener);
    this._eventListeners.set(eventName, listeners);

    return () => {
      listeners.delete(listener);
      if (listeners.size === 0) {
        this._eventListeners.delete(eventName);
      }
    };
  }

  async open(payload) {
    return this._transport.request('open', normalizeOpenPayload(payload));
  }

  async close() {
    return this._transport.request('close');
  }

  async togglePause() {
    return this._transport.request('togglePause');
  }

  async seekTo(seconds) {
    return this._transport.request('seekTo', { seconds: Number(seconds) || 0 });
  }

  async seekRelative(seconds) {
    return this._transport.request('seekRelative', { seconds: Number(seconds) || 0 });
  }

  async setVolume(volume) {
    return this._transport.request('setVolume', { volume: Number(volume) || 0 });
  }

  async setPlaybackSpeed(speed) {
    return this._transport.request('setPlaybackSpeed', { speed: Number(speed) || 1 });
  }

  async setSubtitleId(id) {
    return this._transport.request('setSubtitleId', { id: Number(id) || 0 });
  }

  async send(command, payload) {
    if (typeof command !== 'string' || command.length === 0) {
      throw new TypeError('send(command, payload) expects a non-empty command');
    }

    return this._transport.request(command, payload);
  }

  destroy() {
    if (typeof this._unsubscribeTransport === 'function') {
      this._unsubscribeTransport();
    }
    this._stateListeners.clear();
    this._eventListeners.clear();
  }

  _handleMessage(message) {
    const normalized = normalizeEvent(message);
    if (normalized.type !== 'event') {
      return;
    }

    this._state = applyEventToState(this._state, normalized);

    for (const listener of this._stateListeners) {
      listener(this.getState());
    }

    const listeners = this._eventListeners.get(normalized.name);
    if (listeners) {
      for (const listener of listeners) {
        listener(normalized.payload, this.getState());
      }
    }
  }
}

function createLzcPlayerClient(transport, options) {
  return new LzcPlayerClient(transport, options);
}

function createElectronBridgeTransport(bridge) {
  if (!bridge || typeof bridge.invoke !== 'function' || typeof bridge.subscribe !== 'function') {
    throw new TypeError(
      'createElectronBridgeTransport(bridge) expects an object with invoke(name, payload) and subscribe(listener)'
    );
  }

  return {
    request(name, payload) {
      return bridge.invoke(name, payload);
    },
    subscribe(listener) {
      return bridge.subscribe(listener);
    },
  };
}

function createElectronRendererPlayer(bridge, options) {
  return createLzcPlayerClient(createElectronBridgeTransport(bridge), options);
}

function createMockTransport(initialState) {
  const listeners = new Set();
  let state = createInitialState(initialState);

  function emit(name, payload) {
    const message = { type: 'event', name, payload };
    for (const listener of listeners) {
      listener(message);
    }
  }

  function emitStateSnapshot() {
    emit(EVENT_NAMES.SNAPSHOT, state);
  }

  return {
    request(name, payload) {
      switch (name) {
        case 'open':
          state = {
            ...state,
            sourceUrl: payload.url,
            duration: payload.duration || state.duration || 600,
            playing: !payload.startPaused,
            lastError: null,
          };
          emitStateSnapshot();
          return Promise.resolve({ ok: true });
        case 'close':
          state = createInitialState();
          emitStateSnapshot();
          return Promise.resolve({ ok: true });
        case 'togglePause':
          state = { ...state, playing: !state.playing };
          emit('playing', state.playing);
          return Promise.resolve({ ok: true });
        case 'seekTo':
          state = { ...state, timePos: Number(payload && payload.seconds) || 0 };
          emit(EVENT_NAMES.PROGRESS, {
            timePos: state.timePos,
            duration: state.duration,
            playing: state.playing,
          });
          return Promise.resolve({ ok: true });
        case 'seekRelative':
          state = { ...state, timePos: Math.max(0, state.timePos + (Number(payload && payload.seconds) || 0)) };
          emit(EVENT_NAMES.PROGRESS, {
            timePos: state.timePos,
            duration: state.duration,
            playing: state.playing,
          });
          return Promise.resolve({ ok: true });
        case 'setVolume':
          state = { ...state, volume: Number(payload && payload.volume) || 0 };
          emit('volume', state.volume);
          return Promise.resolve({ ok: true });
        case 'setPlaybackSpeed':
          state = { ...state, playbackSpeed: Number(payload && payload.speed) || 1 };
          emit('playbackSpeed', state.playbackSpeed);
          return Promise.resolve({ ok: true });
        case 'setSubtitleId':
          state = { ...state, subtitleId: Number(payload && payload.id) || 0 };
          emit('subtitleId', state.subtitleId);
          return Promise.resolve({ ok: true });
        default:
          return Promise.resolve({ ok: true, ignored: true });
      }
    },
    subscribe(listener) {
      listeners.add(listener);
      listener({ type: 'event', name: EVENT_NAMES.SNAPSHOT, payload: state });
      return () => {
        listeners.delete(listener);
      };
    },
    emit,
    getState() {
      return cloneState(state);
    },
  };
}

module.exports = {
  EVENT_NAMES,
  LzcPlayerClient,
  createElectronBridgeTransport,
  createElectronRendererPlayer,
  createLzcPlayerClient,
  createMockTransport,
};
