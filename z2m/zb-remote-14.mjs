// Zigbee2MQTT external converter for zb-remote (nRF52840, 14 buttons, 0.91" OLED).
// Buttons: genMultistateInput.presentValue = button*10 + action (1 single, 2 double, 3 hold).
// Display 128x64 two-colour (status bar yellow, 3 tiles in a row blue): cluster 0xFC10, 0..2 = tiles "icon|value", 3 = wake s, 4 = alert "icon|l1|l2|l3", 5 = flip,
// 9 = title, 0x0B = focus (inverted value box), 0x0A = charging (RO, reported by the device on change; battery is not reported while on USB).
// Icons: bell door door_open lock unlock bulb bulb_off temp power battery home moon sun flame snow drop plug fan check cross link search star cam tv button
import * as fz from 'zigbee-herdsman-converters/converters/fromZigbee';
import * as exposes from 'zigbee-herdsman-converters/lib/exposes';
import * as m from 'zigbee-herdsman-converters/lib/modernExtend';
import {Zcl} from 'zigbee-herdsman';

const e = exposes.presets;
const ea = exposes.access;
const ACTIONS = {1: 'single', 2: 'double', 3: 'hold'};
const BUTTONS = 14;
const LINE_BYTES = 48;

const fzAction = {
    cluster: 'genMultistateInput',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        const v = msg.data.presentValue;
        if (typeof v !== 'number') return;
        const btn = Math.floor(v / 10);
        const act = ACTIONS[v % 10];
        if (!act || btn < 1 || btn > BUTTONS) return;
        return {action: `${btn}_${act}`};
    },
};

const fzCharging = {
    cluster: 'zbRemoteDisplay',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        if (msg.data.charging === undefined) return;
        return {charging: msg.data.charging === 1};
    },
};

function utf8Bytes(text) {
    // truncate on a character boundary to LINE_BYTES
    let out = [];
    for (const ch of String(text ?? '')) {
        const b = [...Buffer.from(ch, 'utf8')];
        if (out.length + b.length > LINE_BYTES) break;
        out = out.concat(b);
    }
    return out;
}

// Write coalescing per device: Z2M runs convertSet calls concurrently and herdsman sends them one by
// one; a fast series (☼ ×10) queued every intermediate value, and one "MAC transaction expired"
// (+2 s herdsman retry) made the screen replay stale values for seconds. While a write is in
// flight, newer values only overwrite the pending set; the running loop sends the latest one.
// Values already written with the same content < DEDUP_MS ago are skipped (HA re-renders the whole
// screen on every light/timer change); the window keeps a rebooted remote from staying blank.
const DEDUP_MS = 60 * 1000;
const MAX_FAST_RETRIES = 3;
const pendingWrites = new Map(); // ieee -> attrs
const writing = new Set();
const written = new Map(); // ieee -> {attr: {v, t}}
const requested = new Map(); // ieee -> latest state per key (a call that waited must not publish an older value)

function sameBytes(a, b) {
    return Buffer.isBuffer(a) && Buffer.isBuffer(b) ? a.equals(b) : a === b;
}

async function writeCoalesced(device, ep, attrs) {
    const id = device.ieeeAddr;
    const done = written.get(id) ?? {};
    written.set(id, done);
    const now = Date.now();
    const fresh = {};
    for (const [k, v] of Object.entries(attrs)) {
        const w = done[k];
        if (k !== 'wake' && w && sameBytes(w.v, v) && now - w.t < DEDUP_MS) continue;
        fresh[k] = v;
    }
    if (Object.keys(fresh).length === 0) return;
    pendingWrites.set(id, {...(pendingWrites.get(id) ?? {}), ...fresh});
    if (writing.has(id)) return;
    writing.add(id);
    try {
        let failures = 0;
        while (pendingWrites.has(id)) {
            const next = pendingWrites.get(id);
            pendingWrites.delete(id);
            try {
                // herdsman's recovery waits 2 s on "MAC transaction expired" (240) and then resends the
                // same, by now stale value; the remote polls every 100 ms after a press, so retry at once
                // with the newest values and fall back to the normal recovery only on the last attempt
                await ep.write('zbRemoteDisplay', next, {disableRecovery: failures < MAX_FAST_RETRIES});
                failures = 0;
            } catch (e) {
                if (++failures <= MAX_FAST_RETRIES) {
                    pendingWrites.set(id, {...next, ...(pendingWrites.get(id) ?? {})}); // newer values win
                    continue;
                }
                for (const k of Object.keys(next)) delete done[k];
                throw e;
            }
            const t = Date.now();
            for (const [k, v] of Object.entries(next)) done[k] = {v, t};
        }
    } finally {
        writing.delete(id);
    }
}

// Z2M calls a converter once per message, so handle every display key here in one ZCL write
const STR_KEYS = ['title', 'tile1', 'tile2', 'tile3', 'alert'];
const tzDisplay = {
    key: [...STR_KEYS, 'wake', 'flip', 'offset_x', 'offset_y', 'focus'],
    convertSet: async (entity, key, value, meta) => {
        const ep = meta.device.getEndpoint(1);
        const attrs = {};
        const state = {};
        for (const k of [...STR_KEYS, 'wake', 'flip', 'offset_x', 'offset_y', 'focus']) {
            if (!(k in meta.message)) continue;
            const v = meta.message[k];
            if (k === 'offset_x' || k === 'offset_y') {
                const o = Math.max(-32, Math.min(32, Math.round(Number(v) || 0)));
                attrs[k === 'offset_x' ? 'xoff' : 'yoff'] = o;
                state[k] = o;
            } else if (k === 'focus') {
                const f = Math.max(0, Math.min(3, Math.round(Number(v) || 0)));
                attrs.focus = f;
                state.focus = f;
            } else if (k === 'wake') {
                attrs.wake = Math.max(0, Math.min(255, Number(v) || 0));
            } else if (k === 'flip') {
                const f = v === true || v === 'ON' || v === 1 || v === '1';
                attrs.flip = f ? 1 : 0;
                state.flip = f ? 'ON' : 'OFF';
            } else {
                const bytes = utf8Bytes(v);
                attrs[k] = Buffer.from(bytes);
                state[k] = Buffer.from(bytes).toString('utf8');
            }
        }
        const id = meta.device.ieeeAddr;
        const req = {...(requested.get(id) ?? {}), ...state};
        requested.set(id, req);
        await writeCoalesced(meta.device, ep, attrs);
        const latest = requested.get(id) ?? {};
        return {state: Object.fromEntries(Object.keys(state).map((k) => [k, latest[k] ?? state[k]]))};
    },
};

const actionValues = [];
for (let b = 1; b <= BUTTONS; b++) for (const a of Object.values(ACTIONS)) actionValues.push(`${b}_${a}`);

export default {
    zigbeeModel: ['zb-remote-14'],
    model: 'zb-remote-14',
    vendor: 'heartpolik',
    description: 'DIY 14-button Zigbee remote with OLED (nRF52840)',
    extend: [
        m.deviceAddCustomCluster('zbRemoteDisplay', {
            // herdsman resolves incoming reports via the explicit `name` fields
            name: 'zbRemoteDisplay',
            ID: 0xfc10,
            attributes: {
                tile1: {name: 'tile1', ID: 0x0000, type: Zcl.DataType.OCTET_STR, write: true},
                tile2: {name: 'tile2', ID: 0x0001, type: Zcl.DataType.OCTET_STR, write: true},
                tile3: {name: 'tile3', ID: 0x0002, type: Zcl.DataType.OCTET_STR, write: true},
                wake: {name: 'wake', ID: 0x0003, type: Zcl.DataType.UINT8, write: true},
                alert: {name: 'alert', ID: 0x0004, type: Zcl.DataType.OCTET_STR, write: true},
                flip: {name: 'flip', ID: 0x0005, type: Zcl.DataType.UINT8, write: true},
                xoff: {name: 'xoff', ID: 0x0006, type: Zcl.DataType.INT8, write: true},
                yoff: {name: 'yoff', ID: 0x0007, type: Zcl.DataType.INT8, write: true},
                title: {name: 'title', ID: 0x0009, type: Zcl.DataType.OCTET_STR, write: true},
                charging: {name: 'charging', ID: 0x000a, type: Zcl.DataType.UINT8},
                focus: {name: 'focus', ID: 0x000b, type: Zcl.DataType.UINT8, write: true},
            },
            commands: {},
            commandsResponse: {},
        }),
    ],
    fromZigbee: [fzAction, fz.battery, fzCharging],
    toZigbee: [tzDisplay],
    exposes: [
        e.action(actionValues),
        e.battery(),
        e.battery_voltage(),
        e.binary('charging', ea.STATE, true, false).withDescription('On USB power (battery % is frozen at the last on-battery reading)'),
        e.text('title', ea.STATE_SET).withDescription('Header text in the yellow band (e.g. selected room); empty shows battery %'),
        e.text('tile1', ea.STATE_SET).withDescription('Tile 1: "icon|value", e.g. "temp|23.4°"'),
        e.text('tile2', ea.STATE_SET).withDescription('Tile 2: "icon|value"'),
        e.text('tile3', ea.STATE_SET).withDescription('Tile 3: "icon|value"'),
        e.text('alert', ea.STATE_SET).withDescription('Full-screen alert "icon|title|line2|line3" (title in the yellow band), empty clears'),
        e.numeric('focus', ea.STATE_SET).withValueMin(0).withValueMax(3).withDescription('Tile 1-3 whose value the d-pad changes (shown inverted), 0 = none'),
        e.numeric('offset_x', ea.STATE_SET).withValueMin(-16).withValueMax(16).withUnit('px').withDescription('Shift picture horizontally'),
        e.numeric('offset_y', ea.STATE_SET).withValueMin(-32).withValueMax(32).withUnit('px').withDescription('Shift picture vertically'),
        e.binary('flip', ea.STATE_SET, 'ON', 'OFF').withDescription('Rotate screen 180°'),
        e.numeric('wake', ea.SET).withValueMin(0).withValueMax(255).withUnit('s').withDescription('Turn the screen on for N seconds'),
    ],
    meta: {},
};
