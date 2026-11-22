// Zigbee2MQTT external converter for zb-remote (nRF52840, 14 buttons, 0.91" OLED).
// Buttons: genMultistateInput.presentValue = button*10 + action (1 single, 2 double, 3 hold).
// Display 128x64 two-colour (status bar yellow, 3 tiles in a row blue): cluster 0xFC10, 0..2 = tiles "icon|value", 3 = wake s, 4 = alert "icon|l1|l2|l3", 5 = flip.
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

// Z2M calls a converter once per message, so handle every display key here in one ZCL write
const STR_KEYS = ['title', 'tile1', 'tile2', 'tile3', 'alert'];
const tzDisplay = {
    key: [...STR_KEYS, 'wake', 'flip', 'offset_x', 'offset_y'],
    convertSet: async (entity, key, value, meta) => {
        const ep = meta.device.getEndpoint(1);
        const attrs = {};
        const state = {};
        for (const k of [...STR_KEYS, 'wake', 'flip', 'offset_x', 'offset_y']) {
            if (!(k in meta.message)) continue;
            const v = meta.message[k];
            if (k === 'offset_x' || k === 'offset_y') {
                const o = Math.max(-32, Math.min(32, Math.round(Number(v) || 0)));
                attrs[k === 'offset_x' ? 'xoff' : 'yoff'] = o;
                state[k] = o;
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
        await ep.write('zbRemoteDisplay', attrs);
        return {state};
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
            ID: 0xfc10,
            attributes: {
                tile1: {ID: 0x0000, type: Zcl.DataType.OCTET_STR, write: true},
                tile2: {ID: 0x0001, type: Zcl.DataType.OCTET_STR, write: true},
                tile3: {ID: 0x0002, type: Zcl.DataType.OCTET_STR, write: true},
                wake: {ID: 0x0003, type: Zcl.DataType.UINT8, write: true},
                alert: {ID: 0x0004, type: Zcl.DataType.OCTET_STR, write: true},
                flip: {ID: 0x0005, type: Zcl.DataType.UINT8, write: true},
                xoff: {ID: 0x0006, type: Zcl.DataType.INT8, write: true},
                yoff: {ID: 0x0007, type: Zcl.DataType.INT8, write: true},
                title: {ID: 0x0009, type: Zcl.DataType.OCTET_STR, write: true},
            },
            commands: {},
            commandsResponse: {},
        }),
    ],
    fromZigbee: [fzAction, fz.battery],
    toZigbee: [tzDisplay],
    exposes: [
        e.action(actionValues),
        e.battery(),
        e.battery_voltage(),
        e.text('title', ea.STATE_SET).withDescription('Header text in the yellow band (e.g. selected room); empty shows battery %'),
        e.text('tile1', ea.STATE_SET).withDescription('Tile 1: "icon|value", e.g. "temp|23.4°"'),
        e.text('tile2', ea.STATE_SET).withDescription('Tile 2: "icon|value"'),
        e.text('tile3', ea.STATE_SET).withDescription('Tile 3: "icon|value"'),
        e.text('alert', ea.STATE_SET).withDescription('Full-screen alert "icon|title|line2|line3" (title in the yellow band), empty clears'),
        e.numeric('offset_x', ea.STATE_SET).withValueMin(-16).withValueMax(16).withUnit('px').withDescription('Shift picture horizontally'),
        e.numeric('offset_y', ea.STATE_SET).withValueMin(-32).withValueMax(32).withUnit('px').withDescription('Shift picture vertically'),
        e.binary('flip', ea.STATE_SET, 'ON', 'OFF').withDescription('Rotate screen 180°'),
        e.numeric('wake', ea.SET).withValueMin(0).withValueMax(255).withUnit('s').withDescription('Turn the screen on for N seconds'),
    ],
    meta: {},
};
