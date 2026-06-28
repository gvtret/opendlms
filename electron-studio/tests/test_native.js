/**
 * \file test_native.js
 * \brief Tests for OpenDLMS native addon
 *
 *  Run: node tests/test_native.js
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

'use strict';

const assert = require('assert');
const path = require('path');

let native;
try {
    native = require(path.join(__dirname, '..', 'native', 'build', 'Release', 'opendlms-native.node'));
} catch (e) {
    console.error('Failed to load native addon:', e.message);
    console.error('Build first: cd electron-studio && npm run build:native');
    process.exit(1);
}

let passed = 0;
let failed = 0;

function test(name, fn) {
    try {
        fn();
        passed++;
        console.log(`  PASS: ${name}`);
    } catch (e) {
        failed++;
        console.log(`  FAIL: ${name}`);
        console.log(`        ${e.message}`);
    }
}

console.log('OpenDLMS Native Addon Tests\n');

/* ── Version ─────────────────────────────────────────────────────────────── */

console.log('Version:');

test('VERSION is defined', () => {
    assert.strictEqual(typeof native.VERSION, 'string');
    assert.strictEqual(native.VERSION, '1.1.0');
});

test('Framing constants defined', () => {
    assert.strictEqual(native.FRAMING_NONE, 0);
    assert.strictEqual(native.FRAMING_WRAPPER, 1);
    assert.strictEqual(native.FRAMING_HDLC, 2);
});

test('Security key constants defined', () => {
    assert.strictEqual(native.SEC_KEK, 0);
    assert.strictEqual(native.SEC_GUEK, 1);
    assert.strictEqual(native.SEC_GBEK, 2);
    assert.strictEqual(native.SEC_GAK, 3);
});

test('Security keyring stores 128-bit and 256-bit keys', () => {
    native.clearSecurityKeys();
    assert.strictEqual(native.setSecurityKey(1, native.SEC_GUEK, Buffer.alloc(16, 0x11)), true);
    assert.strictEqual(native.getSecurityKeyLength(1, native.SEC_GUEK), 16);
    assert.strictEqual(native.setSecurityKey(1, native.SEC_GAK, Buffer.alloc(32, 0x22)), true);
    assert.strictEqual(native.getSecurityKeyLength(1, native.SEC_GAK), 32);
    native.clearSecurityKeys();
    assert.strictEqual(native.getSecurityKeyLength(1, native.SEC_GUEK), 0);
});

test('Security keyring rejects invalid key lengths', () => {
    native.clearSecurityKeys();
    assert.strictEqual(native.setSecurityKey(1, native.SEC_GUEK, Buffer.alloc(15, 0x11)), false);
    assert.strictEqual(native.setSecurityKey(1, native.SEC_GUEK, Buffer.alloc(24, 0x11)), false);
    assert.strictEqual(native.getSecurityKeyLength(1, native.SEC_GUEK), 0);
});

/* ── LuaBridge ───────────────────────────────────────────────────────────── */

console.log('\nLuaBridge:');

test('LuaBridge can be instantiated', () => {
    const lb = new native.LuaBridge();
    assert.ok(lb);
    assert.strictEqual(lb.isConnected(), false);
});

test('execReturn: arithmetic expression', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('2 + 2');
    assert.strictEqual(result, '4');
});

test('execReturn: string expression', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('"hello" .. " world"');
    assert.strictEqual(result, 'hello world');
});

test('execReturn: boolean expression', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('true');
    assert.strictEqual(result, 'true');
});

test('execReturn: nil expression', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('nil');
    assert.strictEqual(result, '');
});

test('execReturn: hex function', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('hex("\\x01\\x02\\x03")');
    assert.strictEqual(result, '01 02 03');
});

test('execReturn: obis function', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('type(obis("0.0.1.0.0.255"))');
    assert.strictEqual(result, 'table');
});

test('execReturn: obis rejects malformed input', () => {
    const lb = new native.LuaBridge();
    lb.execReturn('obis("0.0.999.0.0.255")');
    assert.ok(lb.getError().includes('invalid OBIS code'));
    lb.execReturn('obis("0.0.1.0.0.255x")');
    assert.ok(lb.getError().includes('invalid OBIS code'));
});

test('execReturn: delay waits', () => {
    const lb = new native.LuaBridge();
    const start = Date.now();
    const result = lb.execReturn('delay(30)');
    const elapsed = Date.now() - start;
    assert.strictEqual(result, '');
    assert.ok(elapsed >= 20, `delay returned too early: ${elapsed}ms`);
});

test('execReturn: delay rejects negative values', () => {
    const lb = new native.LuaBridge();
    lb.execReturn('delay(-1)');
    assert.ok(lb.getError().includes('delay must be non-negative'));
});

test('execReturn: print captures output', () => {
    const lb = new native.LuaBridge();
    lb.execReturn('print("test output")');
    const output = lb.getOutput();
    assert.ok(output.includes('test output'));
});

test('execReturn: clear output', () => {
    const lb = new native.LuaBridge();
    lb.execReturn('print("to clear")');
    lb.clearOutput();
    const output = lb.getOutput();
    assert.strictEqual(output, '');
});

test('execReturn: error on invalid syntax', () => {
    const lb = new native.LuaBridge();
    lb.execReturn('invalid syntax !!!');
    const err = lb.getError();
    assert.ok(err.length > 0);
});

test('execReturn: multi-line script', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('(function() local a = 10; local b = 20; return a + b end)()');
    assert.strictEqual(result, '30');
});

test('execReturn: function call', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('string.upper("hello")');
    assert.strictEqual(result, 'HELLO');
});

test('execReturn: table serialization', () => {
    const lb = new native.LuaBridge();
    const result = lb.execReturn('string.format("%d.%d.%d", 1, 0, 255)');
    assert.strictEqual(result, '1.0.255');
});

test('execReturn: math operations', () => {
    const lb = new native.LuaBridge();
    assert.ok(lb.execReturn('10 / 3').startsWith('3.3'));
    assert.strictEqual(lb.execReturn('2 ^ 10'), '1024');
});

test('execReturn: string operations', () => {
    const lb = new native.LuaBridge();
    assert.strictEqual(lb.execReturn('#"hello"'), '5');
    assert.strictEqual(lb.execReturn('string.rep("ab", 3)'), 'ababab');
});

test('execReturn: multiple print lines', () => {
    const lb = new native.LuaBridge();
    lb.clearOutput();
    lb.execReturn('print("line1"); print("line2"); print("line3")');
    const output = lb.getOutput();
    const lines = output.split('\n').filter(l => l.trim());
    assert.strictEqual(lines.length, 3);
});

test('execReturn: boolean operations', () => {
    const lb = new native.LuaBridge();
    assert.strictEqual(lb.execReturn('10 > 5'), 'true');
    assert.strictEqual(lb.execReturn('10 < 5'), 'false');
});

/* ── Transport ────────────────────────────────────────────────────────────── */

console.log('\nTransport:');

test('Transport can be instantiated', () => {
    const t = new native.Transport();
    assert.ok(t);
});

test('Transport clientInit returns 0', () => {
    const t = new native.Transport();
    const rc = t.clientInit('127.0.0.1', 4060, native.FRAMING_WRAPPER);
    assert.strictEqual(rc, 0);
    t.destroy();
});

test('Transport serverInit returns 0', () => {
    const t = new native.Transport();
    const rc = t.serverInit(0, native.FRAMING_WRAPPER);
    assert.strictEqual(rc, 0);
    t.destroy();
});

test('Transport client connect to server roundtrip', () => {
    const server = new native.Transport();
    const rcServer = server.serverInit(0, native.FRAMING_WRAPPER);
    assert.strictEqual(rcServer, 0);

    const client = new native.Transport();
    const rcClient = client.clientInit('127.0.0.1', 0, native.FRAMING_WRAPPER);
    assert.strictEqual(rcClient, 0);

    server.destroy();
    client.destroy();
});

test('Transport destroy is safe to call twice', () => {
    const t = new native.Transport();
    t.clientInit('127.0.0.1', 4060, native.FRAMING_WRAPPER);
    t.destroy();
    t.destroy();
});

/* ── Client ─────────────────────────────────────────────────────────────── */

console.log('\nClient:');

test('Client can be instantiated', () => {
    const t = new native.Transport();
    t.clientInit('127.0.0.1', 4060, native.FRAMING_WRAPPER);
    const c = new native.Client(t);
    assert.ok(c);
    c.destroy();
    t.destroy();
});

test('Client get/set/action methods exist', () => {
    const t = new native.Transport();
    t.clientInit('127.0.0.1', 4060, native.FRAMING_WRAPPER);
    const c = new native.Client(t);
    assert.strictEqual(typeof c.get, 'function');
    assert.strictEqual(typeof c.set, 'function');
    assert.strictEqual(typeof c.action, 'function');
    assert.strictEqual(typeof c.getBlock, 'function');
    assert.strictEqual(typeof c.setBlock, 'function');
    assert.strictEqual(typeof c.disconnect, 'function');
    c.destroy();
    t.destroy();
});

/* ── Server ─────────────────────────────────────────────────────────────── */

console.log('\nServer:');

test('Server can be instantiated', () => {
    const t = new native.Transport();
    t.serverInit(0, native.FRAMING_WRAPPER);
    const s = new native.Server(t);
    assert.ok(s);
    s.destroy();
    t.destroy();
});

test('Server poll/send methods exist', () => {
    const t = new native.Transport();
    t.serverInit(0, native.FRAMING_WRAPPER);
    const s = new native.Server(t);
    assert.strictEqual(typeof s.poll, 'function');
    assert.strictEqual(typeof s.send, 'function');
    s.destroy();
    t.destroy();
});

/* ── Data ──────────────────────────────────────────────────────────────── */

console.log('\nData:');

test('Data can be instantiated', () => {
    const d = new native.Data();
    assert.ok(d);
    assert.strictEqual(d.written(), 0);
});

test('Data writeU8 and readU8', () => {
    const d = new native.Data();
    d.writeU8(0x42);
    d.writeU8(0xFF);
    assert.strictEqual(d.written(), 2);
    assert.strictEqual(d.readU8(), 0x42);
    assert.strictEqual(d.readU8(), 0xFF);
});

test('Data writeU16 and readU16', () => {
    const d = new native.Data();
    d.writeU16(0x1234);
    assert.strictEqual(d.written(), 2);
    assert.strictEqual(d.readU16(), 0x1234);
});

test('Data writeU32 and readU32', () => {
    const d = new native.Data();
    d.writeU32(0xDEADBEEF);
    assert.strictEqual(d.written(), 4);
    assert.strictEqual(d.readU32(), 0xDEADBEEF);
});

test('Data writeBuffer and readBuffer', () => {
    const d = new native.Data();
    const src = Buffer.from([0x01, 0x02, 0x03, 0x04]);
    d.writeBuffer(src);
    assert.strictEqual(d.written(), 4);
    const out = d.readBuffer(4);
    assert.ok(Buffer.isBuffer(out));
    assert.deepStrictEqual([...out], [0x01, 0x02, 0x03, 0x04]);
});

test('Data writeBoolean', () => {
    const d = new native.Data();
    d.writeBoolean(true);
    d.writeBoolean(false);
    const buf = d.toBuffer();
    assert.strictEqual(buf.length, 4);
    assert.strictEqual(buf[0], 9);
    assert.strictEqual(buf[1], 1);
    assert.strictEqual(buf[2], 9);
    assert.strictEqual(buf[3], 0);
});

test('Data toBuffer', () => {
    const d = new native.Data();
    d.writeU8(0xAA);
    d.writeU16(0xBBCC);
    const buf = d.toBuffer();
    assert.ok(Buffer.isBuffer(buf));
    assert.strictEqual(buf.length, 3);
    assert.deepStrictEqual([...buf], [0xAA, 0xBB, 0xCC]);
});

test('Data reset', () => {
    const d = new native.Data();
    d.writeU8(0x01);
    assert.strictEqual(d.written(), 1);
    d.reset();
    assert.strictEqual(d.written(), 0);
});

test('Data custom size', () => {
    const d = new native.Data(256);
    assert.strictEqual(d.freeSize(), 256);
});

test('Data unread tracks read position', () => {
    const d = new native.Data();
    d.writeU8(1);
    d.writeU8(2);
    d.writeU8(3);
    d.readU8();
    assert.strictEqual(d.unread(), 2);
});

/* ── Block ─────────────────────────────────────────────────────────────── */

console.log('\nBlock:');

test('Block can be instantiated', () => {
    const b = new native.Block();
    assert.ok(b);
    assert.strictEqual(b.isActive(), false);
});

test('Block init resets state', () => {
    const b = new native.Block();
    b.init();
    assert.strictEqual(b.isActive(), false);
});

test('Block startServer and encodeFirst', () => {
    const b = new native.Block();
    const data = Buffer.alloc(100, 0xAB);
    const rc = b.startServer(1, data);
    assert.strictEqual(rc, 1);
    assert.strictEqual(b.isActive(), true);
    const block = b.encodeFirst(50);
    assert.ok(Buffer.isBuffer(block));
    assert.ok(block.length > 0);
});

test('Block encodeNext returns data', () => {
    const b = new native.Block();
    const data = Buffer.alloc(200, 0xCD);
    b.startServer(1, data);
    b.encodeFirst(50);
    const next = b.encodeNext(50);
    assert.ok(Buffer.isBuffer(next));
});

test('Block abort clears state', () => {
    const b = new native.Block();
    b.startServer(1, Buffer.alloc(10, 0));
    assert.strictEqual(b.isActive(), true);
    b.abort();
    assert.strictEqual(b.isActive(), false);
});

test('Block canReceive requires active state', () => {
    const b = new native.Block();
    assert.strictEqual(b.canReceive(), false);
    b.startReceive(1);
    assert.strictEqual(b.canReceive(), true);
});

/* ── Summary ─────────────────────────────────────────────────────────────── */

console.log(`\n${passed + failed} tests, ${passed} passed, ${failed} failed`);
process.exit(failed > 0 ? 1 : 0);
